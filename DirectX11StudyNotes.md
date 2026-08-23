# LightSaver DirectX 11 학습 기록

이 문서는 `LightSaver`를 직접 구현하면서 공부한 개념, 실제 코드의 데이터 흐름, 사용한 주요 Direct3D 11 함수, 발생한 오류와 원인을 날짜별로 정리한다.

---

## 2026-08-12 — DirectX 11 삼각형 출력

### 오늘의 목표와 결과

오늘의 목표는 Direct3D 11 렌더링 파이프라인을 직접 연결하여 화면에 단색 삼각형 하나를 출력하는 것이었다.

구현한 항목은 다음과 같다.

1. CPU에서 삼각형 정점 세 개 정의
2. 정점 데이터를 보관할 GPU Vertex Buffer 생성
3. HLSL Vertex Shader와 Pixel Shader 작성
4. HLSL 파일을 런타임에 바이트코드로 컴파일
5. 컴파일된 바이트코드로 Direct3D 셰이더 객체 생성
6. C++ 정점 구조와 HLSL 입력을 연결하는 Input Layout 생성
7. Viewport 설정
8. Input Assembler, Vertex Shader, Rasterizer, Pixel Shader, Output Merger 연결
9. `Draw(3, 0)` 호출
10. 정점 나열 순서로 인한 Back-face Culling 문제 진단 및 해결

최종 렌더링 흐름은 다음과 같다.

```text
CPU Vertex 배열
→ ID3D11Buffer(Vertex Buffer)
→ Input Assembler
→ Input Layout에 따라 POSITION0으로 해석
→ Vertex Shader
→ SV_POSITION 출력
→ Rasterizer
→ Pixel Shader
→ SV_TARGET0 출력
→ Output Merger
→ RTV
→ Back Buffer
→ SwapChain::Present
→ 모니터
```

---

### 1. 정점과 삼각형

현재 CPU 정점 구조체는 위치만 가진다.

```cpp
struct Vertex
{
    float x;
    float y;
    float z;
};
```

`float` 하나는 4바이트이므로 `Vertex` 하나의 크기는 12바이트다.

```text
Vertex 한 개

0 byte        4 byte        8 byte       12 byte
├── x ────────┼── y ────────┼── z ────────┤
   float          float          float
```

정점 세 개는 CPU 메모리에 연속해서 놓인다.

```text
Vertex[0] 12 bytes | Vertex[1] 12 bytes | Vertex[2] 12 bytes
총 36 bytes
```

#### 좌표의 의미

현재 Vertex Shader는 행렬 변환 없이 입력 좌표를 그대로 `SV_POSITION`으로 출력한다. 따라서 작성한 좌표는 사실상 Clip Space 좌표로 사용되고, `w = 1`이므로 원근 나눗셈 이후 NDC 좌표와 동일하다.

Direct3D의 NDC 범위는 다음과 같다.

```text
x: -1 ~ 1
y: -1 ~ 1
z:  0 ~ 1
```

- `x = -1`: 화면 왼쪽
- `x = 1`: 화면 오른쪽
- `y = -1`: 화면 아래쪽
- `y = 1`: 화면 위쪽
- `z = 0`: 가까운 깊이 경계
- `z = 1`: 먼 깊이 경계

#### 정점 나열 순서와 컬링

삼각형의 정점은 위치뿐 아니라 나열 순서도 중요하다. Rasterizer는 정점 순서로 삼각형의 앞면과 뒷면을 구분한다.

Direct3D 11 기본 Rasterizer 상태에서는:

- 시계 방향 삼각형을 앞면으로 취급한다.
- `D3D11_CULL_BACK`이 기본값이므로 뒷면은 그리지 않는다.

처음 작성한 정점은 화면에서 반시계 방향이었기 때문에 삼각형이 뒷면으로 판정되어 사라졌다. `Draw()`가 호출되고 배경도 정상 출력됐지만, Rasterizer 단계에서 삼각형이 제거된 것이다.

해결 방법은 다음 중 하나다.

1. 정점 순서를 시계 방향으로 바꾼다.
2. Rasterizer State에서 `FrontCounterClockwise`를 변경한다.
3. 학습 또는 양면 렌더링 목적으로 `CullMode = D3D11_CULL_NONE`을 사용한다.

현재는 가장 기본적인 방법인 정점 순서 변경을 사용한다.

---

### 2. Vertex Buffer

C++ 배열은 CPU 메모리에 존재한다. GPU의 Input Assembler가 정점 데이터를 읽으려면 Direct3D 리소스인 Vertex Buffer가 필요하다.

```cpp
ID3D11Buffer* VertexBuffer = nullptr;
```

`ID3D11Buffer`는 정점, 인덱스, 상수 등 연속된 데이터를 저장할 수 있는 범용 Direct3D 버퍼 리소스다. 버퍼를 만들 때 `BindFlags`로 어떤 파이프라인 용도로 사용할지 지정한다.

#### `D3D11_BUFFER_DESC`

```cpp
D3D11_BUFFER_DESC VertexDesc = {};

VertexDesc.ByteWidth = sizeof(vertices);
VertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
```

주요 필드:

##### `ByteWidth`

버퍼 전체 크기를 바이트 단위로 지정한다.

```cpp
VertexDesc.ByteWidth = sizeof(vertices);
```

현재 정점 하나는 12바이트이고 세 개이므로 총 36바이트다. `sizeof(Vertex)`를 사용하면 정점 하나의 크기만 전달하게 되므로 잘못된 버퍼 크기가 된다.

##### `Usage`

GPU와 CPU가 이 리소스를 어떻게 사용할지 결정한다.

```cpp
VertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
```

`D3D11_USAGE_IMMUTABLE`은 생성 이후 데이터를 변경하지 않는 리소스에 사용한다.

- 생성 시 초기 데이터가 반드시 필요하다.
- CPU에서 갱신할 수 없다.
- 고정된 메시 정점처럼 변경되지 않는 데이터에 적합하다.

다른 주요 Usage:

| Usage | 용도 |
|---|---|
| `D3D11_USAGE_DEFAULT` | GPU가 주로 사용하며 `UpdateSubresource` 등으로 갱신 가능 |
| `D3D11_USAGE_IMMUTABLE` | 생성 후 변경하지 않는 데이터 |
| `D3D11_USAGE_DYNAMIC` | CPU가 자주 갱신하며 보통 `Map/Unmap` 사용 |
| `D3D11_USAGE_STAGING` | CPU-GPU 데이터 복사와 읽기용 |

##### `BindFlags`

버퍼가 파이프라인에서 어떤 역할을 하는지 지정한다.

```cpp
VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
```

`D3D11_BIND_VERTEX_BUFFER`는 이 버퍼가 Input Assembler의 정점 입력으로 연결될 것임을 뜻한다.

#### `D3D11_SUBRESOURCE_DATA`

```cpp
D3D11_SUBRESOURCE_DATA VertexData = {};
VertexData.pSysMem = vertices;
```

`pSysMem`은 버퍼 생성 시 복사할 CPU 데이터의 시작 주소다. `CreateBuffer()`가 호출될 때 CPU 배열의 내용이 Direct3D 버퍼의 초기 데이터로 사용된다.

#### `ID3D11Device::CreateBuffer`

```cpp
HRESULT CreateBuffer(
    const D3D11_BUFFER_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Buffer** ppBuffer
);
```

사용 예:

```cpp
result = Device->CreateBuffer(
    &VertexDesc,
    &VertexData,
    &VertexBuffer
);
```

- `pDesc`: 만들 버퍼의 크기와 사용법
- `pInitialData`: 생성 시 복사할 초기 데이터
- `ppBuffer`: 생성된 버퍼를 받을 출력 포인터
- 반환값: 성공 여부를 나타내는 `HRESULT`

`CreateBuffer()`는 리소스만 생성한다. 이 함수만 호출해서는 렌더링 파이프라인에 연결되지 않는다. 실제 연결은 `IASetVertexBuffers()`가 담당한다.

---

### 3. HLSL 셰이더 구조

현재 셰이더는 `shader.hlsl` 한 파일에 Vertex Shader와 Pixel Shader를 함께 작성한다.

```hlsl
struct VS_INPUT
{
    float3 position : POSITION;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
};
```

#### Vertex Shader

```hlsl
PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);
    return output;
}
```

Vertex Shader는 정점 하나마다 한 번 실행된다.

현재는 World, View, Projection 행렬이 없으므로 입력 좌표를 그대로 출력한다. GPU의 최종 정점 위치는 동차 좌표인 `float4(x, y, z, w)` 형태여야 하므로 `float3`에 `w = 1.0f`를 추가한다.

나중에는 일반적으로 다음 변환을 수행한다.

```text
Local Position
→ World Matrix
→ View Matrix
→ Projection Matrix
→ Clip Space Position(SV_POSITION)
```

#### Pixel Shader

```hlsl
float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
```

Pixel Shader는 Rasterizer가 만든 픽셀 후보마다 실행된다. 현재는 모든 픽셀에 빨간색 RGBA 값을 반환한다.

```text
R = 1
G = 0
B = 0
A = 1
```

---

### 4. Semantic

Semantic은 HLSL 변수 이름이 아니라 파이프라인을 통과하는 데이터의 의미를 나타내는 이름표다.

```hlsl
float3 position : POSITION;
```

- `position`: HLSL 함수 내부에서 사용하는 변수 이름
- `POSITION`: Input Layout과 연결되는 Semantic

따라서 변수 이름을 바꾸더라도 Semantic이 같다면 파이프라인 연결에는 영향을 주지 않는다.

```hlsl
float3 anyName : POSITION;
```

#### `POSITION`과 `POSITION0`

Semantic Index를 생략하면 인덱스 0으로 취급된다.

```text
POSITION == POSITION0
```

C++ Input Layout의 다음 두 값과 연결된다.

```cpp
SemanticName = "POSITION";
SemanticIndex = 0;
```

#### `SV_POSITION`

`SV_`는 System Value를 의미한다. 사용자가 임의로 정의한 일반 Semantic과 달리 GPU 파이프라인이 특별한 의미로 처리한다.

Vertex Shader 출력의 `SV_POSITION`은 Rasterizer가 삼각형을 만들 때 사용할 최종 Clip Space 위치다. Vertex Shader는 래스터라이징되는 도형을 그리려면 최종 위치를 `SV_POSITION`으로 출력해야 한다.

Pixel Shader 입력의 `SV_POSITION`은 Rasterizer가 계산한 화면상의 픽셀 위치를 의미한다. 현재 Pixel Shader는 이 값을 직접 사용하지 않지만 Vertex Shader 출력과 Rasterizer의 데이터 흐름을 표현하기 위해 입력 구조체에 포함되어 있다.

#### `SV_TARGET`

Pixel Shader 반환값의 `SV_TARGET`은 반환된 색상을 Render Target 0에 기록하라는 의미다.

```text
SV_TARGET == SV_TARGET0
```

여러 Render Target에 동시에 출력하는 MRT에서는 `SV_TARGET0`, `SV_TARGET1`처럼 구분한다.

현재 Semantic 연결:

```text
C++ Input Layout "POSITION", 0
↕
HLSL VS_INPUT.position : POSITION0
↓
HLSL PS_INPUT.position : SV_POSITION
↓
Rasterizer
↓
Pixel Shader return : SV_TARGET0
↓
현재 Output Merger에 연결된 RTV
```

---

### 5. 셰이더 컴파일과 객체 생성

HLSL 소스 코드는 GPU가 직접 실행할 수 없다. 먼저 셰이더 바이트코드로 컴파일하고, 그 바이트코드로 Direct3D 셰이더 객체를 생성해야 한다.

```text
shader.hlsl
→ D3DCompileFromFile
→ ID3DBlob에 저장된 셰이더 바이트코드
→ CreateVertexShader / CreatePixelShader
→ ID3D11VertexShader / ID3D11PixelShader
```

#### `ID3DBlob`

`ID3DBlob`은 크기가 정해지지 않은 바이너리 데이터 덩어리를 보관하는 COM 객체다. 셰이더 컴파일 결과와 컴파일 오류 메시지를 담는 데 사용한다.

주요 함수:

```cpp
void* GetBufferPointer();
SIZE_T GetBufferSize();
```

- `GetBufferPointer()`: 데이터가 시작되는 메모리 주소
- `GetBufferSize()`: 데이터의 바이트 크기

#### `D3DCompileFromFile`

```cpp
HRESULT D3DCompileFromFile(
    LPCWSTR pFileName,
    const D3D_SHADER_MACRO* pDefines,
    ID3DInclude* pInclude,
    LPCSTR pEntrypoint,
    LPCSTR pTarget,
    UINT Flags1,
    UINT Flags2,
    ID3DBlob** ppCode,
    ID3DBlob** ppErrorMsgs
);
```

Vertex Shader 컴파일 예:

```cpp
result = D3DCompileFromFile(
    L"shader.hlsl",
    nullptr,
    D3D_COMPILE_STANDARD_FILE_INCLUDE,
    "VS_Main",
    "vs_5_0",
    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
    0,
    &VSBlob,
    &ErrBlob
);
```

주요 인자:

- `pFileName`: HLSL 파일 경로
- `pEntrypoint`: 셰이더 시작 함수 이름
- `pTarget`: 셰이더 종류와 Shader Model
- `vs_5_0`: Vertex Shader Model 5.0
- `ps_5_0`: Pixel Shader Model 5.0
- `ppCode`: 성공 시 컴파일된 바이트코드
- `ppErrorMsgs`: 실패 또는 경고 시 컴파일러 메시지

`L"shader.hlsl"`은 상대경로다. 상대경로는 실행 파일의 위치가 아니라 프로세스의 현재 작업 디렉터리를 기준으로 해석된다. Visual Studio 디버깅 작업 디렉터리를 `$(ProjectDir)`로 지정하면 프로젝트 폴더의 `shader.hlsl`을 찾을 수 있다.

#### `ID3D11Device::CreateVertexShader`

```cpp
HRESULT CreateVertexShader(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11VertexShader** ppVertexShader
);
```

컴파일된 VS Blob의 주소와 크기를 전달해 실제 Vertex Shader 객체를 생성한다.

```cpp
result = Device->CreateVertexShader(
    VSBlob->GetBufferPointer(),
    VSBlob->GetBufferSize(),
    nullptr,
    &VS
);
```

#### `ID3D11Device::CreatePixelShader`

사용법은 Vertex Shader 생성과 같으며 PS Blob을 사용한다.

```cpp
result = Device->CreatePixelShader(
    PSBlob->GetBufferPointer(),
    PSBlob->GetBufferSize(),
    nullptr,
    &PS
);
```

셰이더 객체 생성 후 Blob은 GPU 셰이더 객체와 별개의 임시 CPU 메모리다. 단, VS Blob은 Input Layout 생성 시 Vertex Shader의 입력 서명을 검사하는 데 한 번 더 필요하므로 Input Layout 생성 이후 해제해야 한다.

```text
VS 컴파일
→ VS 객체 생성
→ VS Blob으로 Input Layout 생성
→ VS Blob Release
```

---

### 6. Input Layout

Vertex Buffer는 바이트 배열일 뿐이다. Input Assembler는 Input Layout을 통해 각 바이트를 어떤 타입과 의미로 해석해야 하는지 알 수 있다.

현재 레이아웃:

```cpp
D3D11_INPUT_ELEMENT_DESC layout[] =
{
    {
        "POSITION",
        0,
        DXGI_FORMAT_R32G32B32_FLOAT,
        0,
        0,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
};
```

`D3D11_INPUT_ELEMENT_DESC` 필드:

#### `SemanticName`

```cpp
"POSITION"
```

HLSL 입력의 `POSITION` Semantic과 연결된다.

#### `SemanticIndex`

```cpp
0
```

`POSITION0`을 뜻한다. 같은 의미의 데이터가 여러 개일 때 인덱스로 구분할 수 있다.

#### `Format`

```cpp
DXGI_FORMAT_R32G32B32_FLOAT
```

32비트 실수 세 개를 읽는다는 뜻이다.

```text
R → x
G → y
B → z
```

여기서 R/G/B는 반드시 색상을 뜻하는 것이 아니다. DXGI Format이 구성 요소를 표현할 때 사용하는 이름이다. HLSL의 `float3` 및 C++의 `float x, y, z`와 크기와 타입이 일치한다.

#### `InputSlot`

```cpp
0
```

Input Assembler의 Vertex Buffer 슬롯 0에서 이 데이터를 읽는다. `IASetVertexBuffers()`의 첫 번째 인자와 연결된다.

#### `AlignedByteOffset`

```cpp
0
```

정점 하나의 시작 위치에서 몇 바이트 떨어진 곳부터 이 요소가 시작하는지를 뜻한다. 위치가 정점의 첫 필드이므로 0이다.

색상 필드가 위치 뒤에 추가된다면 위치가 12바이트이므로 색상의 Offset은 12가 된다.

#### `InputSlotClass`

```cpp
D3D11_INPUT_PER_VERTEX_DATA
```

정점이 바뀔 때마다 다음 데이터를 읽는다. 인스턴싱 데이터라면 `D3D11_INPUT_PER_INSTANCE_DATA`를 사용한다.

#### `InstanceDataStepRate`

```cpp
0
```

현재 인스턴싱을 사용하지 않으므로 0이다.

#### `ID3D11Device::CreateInputLayout`

```cpp
HRESULT CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
    UINT NumElements,
    const void* pShaderBytecodeWithInputSignature,
    SIZE_T BytecodeLength,
    ID3D11InputLayout** ppInputLayout
);
```

Vertex Shader 바이트코드에는 셰이더가 요구하는 입력 서명이 들어 있다. Direct3D는 C++ Input Layout의 `POSITION0 float3`와 Vertex Shader의 입력 서명이 호환되는지 확인한 뒤 Input Layout 객체를 생성한다.

---

### 7. Viewport

Viewport는 NDC 좌표를 Render Target의 픽셀 영역으로 변환할 범위를 정한다.

```cpp
D3D11_VIEWPORT Viewport = {};
Viewport.TopLeftX = 0.0f;
Viewport.TopLeftY = 0.0f;
Viewport.Width = 1280.0f;
Viewport.Height = 720.0f;
Viewport.MinDepth = 0.0f;
Viewport.MaxDepth = 1.0f;
```

```text
NDC x: -1 ~ 1 → Pixel x: 0 ~ 1280
NDC y: -1 ~ 1 → Pixel y: 720 ~ 0
NDC z:  0 ~ 1 → Depth:   0 ~ 1
```

`RSSetViewports()`로 Rasterizer Stage에 적용한다.

```cpp
DeviceContext->RSSetViewports(1, &Viewport);
```

Viewport는 Back Buffer 자체를 나누는 리소스가 아니다. 같은 Render Target 안에서 셰이더 출력 좌표를 배치할 픽셀 영역을 정하는 상태다. 여러 Viewport를 사용하면 분할 화면이나 미니맵 같은 구성이 가능하다.

---

### 8. 렌더링 파이프라인 연결 함수

#### `OMSetRenderTargets`

```cpp
DeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
```

Output Merger에 색상 출력 대상인 RTV를 연결한다.

- 첫 번째 인자: 연결할 RTV 개수
- 두 번째 인자: RTV 배열
- 세 번째 인자: Depth Stencil View, 현재는 사용하지 않음

#### `ClearRenderTargetView`

```cpp
DeviceContext->ClearRenderTargetView(RTV, clearColor);
```

Render Target 전체를 지정한 RGBA 색으로 초기화한다. 이것은 Draw Call이 아니라 명시적인 화면 초기화 명령이다.

#### `RSSetViewports`

```cpp
DeviceContext->RSSetViewports(1, &Viewport);
```

Rasterizer가 사용할 Viewport를 설정한다.

#### `IASetInputLayout`

```cpp
DeviceContext->IASetInputLayout(InputLayout);
```

Vertex Buffer의 바이트를 어떤 Semantic과 Format으로 읽을지 지정한다.

#### `IASetVertexBuffers`

```cpp
UINT stride = sizeof(Vertex);
UINT offset = 0;

DeviceContext->IASetVertexBuffers(
    0,
    1,
    &VertexBuffer,
    &stride,
    &offset
);
```

인자 의미:

- `StartSlot = 0`: IA 슬롯 0부터 연결
- `NumBuffers = 1`: 버퍼 한 개 연결
- `ppVertexBuffers`: 연결할 Vertex Buffer 배열
- `pStrides`: 정점 하나를 읽은 후 다음 정점으로 이동할 바이트 수
- `pOffsets`: 버퍼 시작 위치에서 읽기를 시작할 바이트 Offset

현재 `stride`는 `sizeof(Vertex)`, 즉 12바이트다.

#### `IASetPrimitiveTopology`

```cpp
DeviceContext->IASetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
);
```

정점들을 어떤 도형으로 조립할지 정한다. `TRIANGLELIST`는 정점 세 개마다 독립적인 삼각형 하나를 만든다.

```text
Vertex 0, 1, 2 → Triangle 0
Vertex 3, 4, 5 → Triangle 1
```

#### `VSSetShader`

```cpp
DeviceContext->VSSetShader(VS, nullptr, 0);
```

현재 Vertex Shader Stage에서 실행할 셰이더를 연결한다.

#### `PSSetShader`

```cpp
DeviceContext->PSSetShader(PS, nullptr, 0);
```

현재 Pixel Shader Stage에서 실행할 셰이더를 연결한다.

#### `Draw`

```cpp
DeviceContext->Draw(3, 0);
```

- 첫 번째 인자 `VertexCount = 3`: 사용할 정점 개수
- 두 번째 인자 `StartVertexLocation = 0`: Vertex Buffer의 0번 정점부터 시작

`Draw()` 자체가 Vertex Buffer, Input Layout, Shader를 인자로 받지 않는 이유는 이 객체들이 이미 DeviceContext의 파이프라인 상태로 연결되어 있기 때문이다.

#### `Present`

```cpp
SwapChain->Present(1, 0);
```

렌더링이 끝난 Back Buffer를 화면에 표시한다.

- 첫 번째 인자 `1`: 수직 동기화 간격 1
- 두 번째 인자 `0`: 추가 Present Flag 없음

---

### 9. Device와 DeviceContext의 역할 구분

이번 구현에서 두 객체의 차이가 명확하게 드러난다.

#### Device

리소스와 상태 객체를 생성한다.

```text
CreateBuffer
CreateVertexShader
CreatePixelShader
CreateInputLayout
CreateRenderTargetView
```

#### DeviceContext

생성된 객체를 파이프라인에 연결하고 렌더링 명령을 전달한다.

```text
OMSetRenderTargets
RSSetViewports
IASetInputLayout
IASetVertexBuffers
IASetPrimitiveTopology
VSSetShader
PSSetShader
Draw
ClearRenderTargetView
```

요약하면:

```text
Device = 생성 담당
DeviceContext = 연결과 명령 담당
```

---

### 10. 오늘 발생한 오류와 원인

#### 오류 1: Vertex Buffer 크기를 정점 하나의 크기로 덮어씀

잘못된 형태:

```cpp
VertexDesc.ByteWidth = sizeof(vertices);
VertexDesc.ByteWidth = sizeof(Vertex);
```

두 번째 대입으로 전체 36바이트가 아니라 12바이트만 지정된다. 두 번째 줄은 원래 버퍼 용도를 지정하는 다음 코드여야 했다.

```cpp
VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
```

#### 오류 2: Visual Studio HLSL 자동 컴파일이 `main`을 찾음

빌드 오류:

```text
FXC error X3501: 'main': entrypoint not found
```

HLSL 파일에 `VS_Main`과 `PS_Main`이 있지만 Visual Studio의 `<FxCompile>` 자동 빌드는 기본 Entry Point인 `main`을 찾았다. 현재 프로젝트는 C++의 `D3DCompileFromFile()`에서 Entry Point를 직접 지정하는 런타임 컴파일 방식을 사용한다.

Debug x64 구성에서는 `shader.hlsl`을 빌드에서 제외해 중복 컴파일을 피했다.

핵심 차이:

```text
Visual Studio 빌드 시 HLSL 컴파일
vs.
프로그램 실행 중 D3DCompileFromFile로 HLSL 컴파일
```

현재는 두 번째 방식을 학습하고 있다.

#### 오류 3: 상대경로로 HLSL 파일을 찾지 못함

```cpp
D3DCompileFromFile(L"shader.hlsl", ...);
```

상대경로는 현재 작업 디렉터리를 기준으로 한다. 작업 디렉터리가 `x64/Debug`이면 해당 폴더에 HLSL이 없어 컴파일에 실패하고, 프로젝트 디렉터리라면 성공한다.

Visual Studio 디버깅 작업 디렉터리를 다음으로 설정한다.

```text
$(ProjectDir)
```

#### 오류 4: 배경만 나오고 삼각형이 보이지 않음

셰이더, Semantic, Input Layout은 정상이었지만 정점 나열 순서가 반시계 방향이었다. 기본 Rasterizer가 이를 뒷면으로 판단하고 Back-face Culling으로 제거했다.

```text
ClearRenderTargetView 성공
Draw 호출 성공
Rasterizer에서 삼각형 컬링
Present 성공
결과: 배경만 보임
```

정점을 시계 방향으로 재배열하여 해결했다.

이 사례는 화면에 아무것도 나오지 않는 문제가 반드시 셰이더 오류 때문은 아니라는 점을 보여준다. 렌더링 디버깅에서는 다음 단계를 순서대로 확인해야 한다.

```text
리소스 생성 성공 여부
→ 파이프라인 바인딩
→ Input Layout과 Semantic
→ Primitive Topology
→ Viewport
→ 정점 위치와 Winding Order
→ Rasterizer Culling
→ Pixel Shader 출력
→ RTV와 Present
```

---

### 11. COM 객체와 수명 관리

Direct3D 11 인터페이스 대부분은 COM 객체다. 사용이 끝나면 `Release()`를 호출해 참조 횟수를 줄여야 한다.

생성의 역순으로 해제하는 것이 기본 원칙이다.

```text
Device / DeviceContext / SwapChain 생성
→ RTV 생성
→ Vertex Buffer 생성
→ Shader 생성
→ Input Layout 생성

해제는 대체로 역순

Input Layout
→ Pixel Shader
→ Vertex Shader
→ Vertex Buffer
→ RTV
→ DeviceContext
→ SwapChain
→ Device
```

Blob은 임시 컴파일 결과이므로 셰이더 객체와 필요한 Input Layout을 만든 뒤 해제한다.

향후에는 `Microsoft::WRL::ComPtr<T>`를 사용하면 조기 반환이 발생해도 자동으로 `Release()`되어 누수 위험을 줄일 수 있다. 하지만 현재 단계에서는 COM 참조 횟수와 객체 수명을 직접 이해하기 위해 원시 포인터와 `Release()`를 사용하고 있다.

---

### 12. 현재 코드에서 다음에 정리할 안전성 항목

현재 삼각형 출력 학습 목표는 달성했지만 다음 항목은 구조 분리 전에 정리할 필요가 있다.

1. `CreateBuffer()` 반환 `HRESULT` 검사
2. `CreateInputLayout()` 반환 `HRESULT` 검사
3. `ErrBlob`의 메시지를 `OutputDebugStringA()`로 출력
4. `ErrBlob`이 생성된 모든 경로에서 `Release()`
5. `InputLayout`, `VS`, `PS` 종료 시 `Release()`
6. 초기화 중 실패해 조기 반환하는 모든 경로에서 이미 생성한 COM 객체 해제
7. `D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST` 대신 Direct3D 11 이름인 `D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST` 사용
8. Debug x64뿐 아니라 다른 빌드 구성에서도 HLSL 컴파일 방식을 일관되게 설정
9. 창 클라이언트 크기와 Swap Chain/Viewport 크기를 일치시키고 Resize 처리 추가
10. 장기적으로 `ComPtr`과 초기화/종료 함수 또는 클래스로 수명 관리 분리

이 항목들은 다음 리팩터링 단계에서 처리한다.

---

### 오늘의 핵심 요약

```text
Vertex 배열은 CPU 데이터다.
Vertex Buffer는 GPU 파이프라인이 사용할 Direct3D 리소스다.
Input Layout은 정점 바이트의 해석 방법이다.
Semantic은 셰이더 단계 사이에서 데이터 의미를 연결하는 이름표다.
VS Blob은 컴파일된 바이트코드이며 Input Layout 검증에도 사용된다.
Vertex Shader는 최종 정점 위치를 SV_POSITION으로 출력한다.
Rasterizer는 정점으로 삼각형과 픽셀 후보를 만든다.
Pixel Shader는 SV_TARGET으로 Render Target 색상을 출력한다.
Device는 리소스를 만들고 DeviceContext는 파이프라인에 연결하고 명령한다.
Draw는 이미 연결된 파이프라인 상태를 이용해 정점을 처리한다.
정점 Winding Order가 잘못되면 정상적인 Draw Call도 컬링되어 보이지 않을 수 있다.
```

---

## 2026-08-13 — Index Buffer부터 회전하는 3D 큐브까지

### 오늘의 목표와 결과

어제는 정점 세 개를 화면에 그대로 출력했다. 오늘은 같은 렌더링 파이프라인을 실제 3D 공간으로 확장했다.

구현한 항목은 다음과 같다.

1. 큐브의 고유 위치 정점 8개 작성
2. 큐브의 삼각형 12개를 나타내는 인덱스 36개 작성
3. Index Buffer 생성과 `DrawIndexed()` 적용
4. Depth Texture와 Depth Stencil View 생성
5. 매 프레임 Depth Buffer 초기화 및 깊이 검사 적용
6. DirectXMath를 사용한 World, View, Projection 행렬 생성
7. Camera Buffer와 Object Buffer 분리
8. `D3D11_USAGE_DYNAMIC` Constant Buffer 생성
9. `Map()`과 `Unmap()`으로 행렬 데이터를 GPU에 전달
10. HLSL에서 Local → World → View → Projection 변환
11. `deltaTime`을 이용한 Y축 회전

최종 데이터 흐름은 다음과 같다.

```text
CPU의 큐브 정점 8개
→ Vertex Buffer

CPU의 인덱스 36개
→ Index Buffer

CPU에서 DirectXMath로 World/View/Projection 계산
→ Dynamic Constant Buffer를 Map
→ 행렬 저장
→ Unmap
→ Vertex Shader의 b0, b1 슬롯

DrawIndexed(36, 0, 0)
→ Index Buffer에서 정점 번호 조회
→ Vertex Buffer에서 실제 정점 데이터 조회
→ Local → World → View → Projection
→ Rasterizer
→ Depth Test
→ Back Buffer
→ Present
```

---

### 1. DirectXMath와 Direct3D 11은 무엇이 다른가

DirectX는 하나의 단일 기능이 아니라 여러 API와 라이브러리의 묶음이다.

```text
Direct3D 11
→ 버퍼와 텍스처 같은 GPU 리소스를 생성한다.
→ 셰이더와 리소스를 그래픽 파이프라인에 연결한다.
→ Draw 명령을 그래픽 드라이버와 GPU에 전달한다.

DirectXMath
→ CPU에서 벡터와 행렬을 계산한다.
→ 가능한 경우 CPU의 SIMD 명령을 활용한다.
→ GPU 리소스를 만들거나 화면에 직접 그리지는 않는다.
```

예를 들어 다음 코드는 CPU에서 실행된다.

```cpp
XMMATRIX world = XMMatrixRotationY(rotation);
XMMATRIX view = XMMatrixLookAtLH(position, target, up);
XMMATRIX projection = XMMatrixPerspectiveFovLH(
    XM_PIDIV4,
    1280.0f / 720.0f,
    0.1f,
    100.0f
);
```

반면 다음 코드는 Direct3D 11을 통해 GPU가 사용할 리소스와 명령을 설정한다.

```cpp
Device->CreateBuffer(...);
DeviceContext->VSSetConstantBuffers(...);
DeviceContext->DrawIndexed(...);
```

그리고 HLSL의 Vertex Shader는 GPU에서 실행된다.

```hlsl
float4 worldPosition = mul(localPosition, World);
float4 viewPosition = mul(worldPosition, View);
output.position = mul(viewPosition, Projection);
```

CPU에서 모든 정점을 직접 변환해 매 프레임 GPU에 다시 보내는 것이 아니다. CPU는 물체와 카메라를 나타내는 작은 행렬 몇 개를 계산해 전달하고, GPU가 그 행렬을 수많은 정점에 병렬로 적용한다.

```text
CPU
게임 상태와 카메라 상태 계산
→ 행렬 몇 개 생성

GPU
전달받은 행렬을 모든 정점에 적용
→ 삼각형 생성
→ 픽셀 계산
→ 깊이 검사
```

---

### 2. SIMD란 무엇인가

SIMD는 Single Instruction, Multiple Data의 약자다. 하나의 CPU 명령으로 여러 숫자를 동시에 계산하는 방식이다.

일반적인 `float` 네 개를 하나씩 계산한다고 생각하면 다음과 같다.

```text
x 계산
y 계산
z 계산
w 계산
```

128비트 SIMD 레지스터에는 32비트 `float` 네 개를 함께 담을 수 있다.

```text
┌────────┬────────┬────────┬────────┐
│   x    │   y    │   z    │   w    │
└────────┴────────┴────────┴────────┘
```

따라서 벡터 덧셈, 곱셈 및 행렬 계산에서 여러 성분을 한 번에 처리할 수 있다. 3D 그래픽은 위치 `(x, y, z, w)`, 색상 `(r, g, b, a)`, 4×4 행렬처럼 같은 종류의 숫자를 반복해서 계산하므로 SIMD와 잘 맞는다.

DirectXMath에서 대표적인 SIMD 계산용 자료형은 다음과 같다.

```cpp
XMVECTOR // float 네 개에 해당하는 벡터 계산용 자료형
XMMATRIX // XMVECTOR 네 개에 해당하는 4×4 행렬 계산용 자료형
```

SIMD는 CPU 기능이다. GPU의 병렬 연산과는 다른 개념이다.

```text
CPU SIMD
→ 하나의 CPU 명령으로 여러 숫자를 처리

GPU 병렬 처리
→ 매우 많은 정점 또는 픽셀에 대해 셰이더 실행을 동시에 진행
```

---

### 3. 저장용 자료형과 계산용 자료형

DirectXMath는 데이터를 오래 저장하기 위한 자료형과 계산하기 위한 자료형을 구분한다.

| 저장용 | 계산용 | 의미 |
|---|---|---|
| `XMFLOAT2` | `XMVECTOR` | 2차원 데이터 |
| `XMFLOAT3` | `XMVECTOR` | 3차원 데이터 |
| `XMFLOAT4` | `XMVECTOR` | 4차원 데이터 |
| `XMFLOAT4X4` | `XMMATRIX` | 4×4 행렬 |

`XMFLOAT4`와 `XMVECTOR`는 개념적으로 모두 숫자 네 개를 표현하지만 용도가 다르다.

```text
XMFLOAT4
→ 일반 메모리에 값을 보관하기 좋은 저장 상자
→ x, y, z, w 멤버에 직접 접근 가능

XMVECTOR
→ SIMD를 이용해 계산하기 위한 작업 도구
→ DirectXMath 함수에 전달하여 계산
```

저장된 값을 계산하려면 Load하고, 계산 결과를 저장하려면 Store한다.

```cpp
XMFLOAT4 storedValue = { 1.0f, 2.0f, 3.0f, 1.0f };

XMVECTOR calculatingValue = XMLoadFloat4(&storedValue);

calculatingValue = XMVectorScale(calculatingValue, 2.0f);

XMStoreFloat4(&storedValue, calculatingValue);
```

행렬도 같은 흐름이다.

```cpp
XMFLOAT4X4 storedMatrix;

XMMATRIX calculatingMatrix = XMLoadFloat4x4(&storedMatrix);

calculatingMatrix = calculatingMatrix * anotherMatrix;

XMStoreFloat4x4(&storedMatrix, calculatingMatrix);
```

현재 프로젝트에서는 `XMMATRIX`로 행렬을 계산한 후, Constant Buffer 내부의 `XMFLOAT4X4`에 저장한다.

```cpp
XMStoreFloat4x4(
    &cameraData->View,
    XMMatrixTranspose(view)
);
```

---

### 4. 메모리 정렬과 Constant Buffer 정렬은 다르다

`alignas(16)`은 C++11의 표준 키워드다.

```cpp
struct alignas(16) CameraBufferData
{
    XMFLOAT4X4 View;
    XMFLOAT4X4 Projection;
};
```

이 선언은 구조체 객체의 시작 주소를 16바이트 경계에 맞추도록 요구한다.

```text
16바이트 경계 주소
0, 16, 32, 48, 64, ...
```

정렬된 주소는 SIMD 데이터가 메모리 또는 캐시 경계를 불필요하게 가로지르는 경우를 줄이고, 정렬을 요구하는 연산을 안전하게 사용하는 데 도움이 된다.

그러나 다음 두 조건은 별개다.

```text
alignas(16)
→ CPU 메모리에서 구조체 시작 주소의 정렬

sizeof(BufferData) % 16 == 0
→ Direct3D Constant Buffer 전체 크기 조건
```

현재 구조체의 크기는 다음과 같다.

```text
XMFLOAT4X4 = float 16개 = 64바이트

CameraBufferData
View 64 + Projection 64 = 128바이트

ObjectBufferData
World 64 = 64바이트
```

둘 다 16바이트 배수다. 컴파일 시점에 조건을 검사한다.

```cpp
static_assert(sizeof(CameraBufferData) % 16 == 0);
static_assert(sizeof(ObjectBufferData) % 16 == 0);
```

---

### 5. 동차 좌표와 `w`

3D 그래픽에서는 위치를 `(x, y, z)`가 아니라 `(x, y, z, w)` 형태로 계산한다. 네 번째 값 `w`는 위치와 방향을 구분하고, 이동을 포함한 여러 변환을 하나의 행렬 곱셈으로 처리할 수 있게 한다.

```text
(x, y, z, 1) → 위치 Point
(x, y, z, 0) → 방향 Direction
```

카메라 설정을 예로 들면 다음과 같다.

```cpp
XMVECTOR cameraPosition = XMVectorSet(0, 0, -3, 1);
XMVECTOR cameraTarget   = XMVectorSet(0, 0,  0, 1);
XMVECTOR cameraUp       = XMVectorSet(0, 1,  0, 0);
```

`cameraPosition`과 `cameraTarget`은 실제 공간에 존재하는 점이므로 `w = 1`이다. `cameraUp`은 특정 위치가 아니라 위쪽을 나타내는 방향이므로 `w = 0`이다.

이 규칙은 벡터 계산에서도 자연스럽게 이어진다.

```text
점 - 점 = 방향
(w = 1) - (w = 1) = (w = 0)

점 + 방향 = 새로운 점
(w = 1) + (w = 0) = (w = 1)

방향 + 방향 = 새로운 방향
(w = 0) + (w = 0) = (w = 0)
```

이동 행렬의 이동 성분에는 `w`가 곱해진다. 따라서 위치는 이동하지만 방향은 이동하지 않는다.

```text
위치 (1, 0, 0, 1)에 x축 이동 10 적용
→ (11, 0, 0, 1)

방향 (1, 0, 0, 0)에 x축 이동 10 적용
→ (1, 0, 0, 0)
```

방향은 공간상의 장소가 아니므로 카메라나 물체가 이동한다고 함께 평행 이동하면 안 된다.

---

### 6. 벡터 길이, 거리, 정규화

벡터 `(x, y, z)`의 길이는 피타고라스 정리로 구한다.

```text
길이 = √(x² + y² + z²)
```

DirectXMath 함수는 다음과 같다.

```cpp
XMVECTOR length = XMVector3Length(vector);
XMVECTOR lengthSq = XMVector3LengthSq(vector);
```

두 위치 A와 B 사이의 거리는 먼저 방향 벡터를 만든 뒤 그 길이를 구한다.

```cpp
XMVECTOR aToB = XMVectorSubtract(positionB, positionA);
XMVECTOR distance = XMVector3Length(aToB);
```

단순히 어느 거리가 더 짧은지 비교할 때는 제곱근을 계산할 필요가 없다.

```text
distance² < radius²
```

```cpp
XMVECTOR distanceSq = XMVector3LengthSq(aToB);
```

정규화는 벡터의 방향을 유지하면서 길이를 1로 만드는 것이다.

```cpp
XMVECTOR direction = XMVector3Normalize(aToB);
```

이렇게 방향과 속력을 분리할 수 있다.

```text
Velocity = Direction × Speed
```

길이가 0인 벡터는 정규화할 수 없다. 0으로 나누는 문제가 생기므로 입력 벡터의 길이를 확인해야 한다.

---

### 7. 내적: 두 방향이 얼마나 같은 쪽을 보는가

내적 공식은 다음과 같다.

```text
A · B = |A||B|cosθ
```

두 벡터를 정규화했다면 길이가 모두 1이므로 결과는 `cosθ`가 된다.

```cpp
XMVECTOR dot = XMVector3Dot(directionA, directionB);
```

정규화된 벡터의 내적 결과를 직관적으로 해석하면 다음과 같다.

```text
 1  → 완전히 같은 방향, 0도
 0  → 서로 직각, 90도
-1  → 완전히 반대 방향, 180도
```

LightSaver의 손전등 원뿔 판정에도 사용할 수 있다.

```text
FlashlightForward = 손전등이 바라보는 단위 방향
DirectionToMonster = 손전등에서 몬스터로 향하는 단위 방향

dot = FlashlightForward · DirectionToMonster

dot >= cos(손전등 반각)
→ 몬스터가 손전등 원뿔 안에 있음
```

거리 조건까지 함께 사용하면 실제 손전등 범위가 된다.

```text
거리 <= 손전등 사거리
그리고
내적 >= cos(손전등 반각)
```

내적은 조명에서도 사용한다. 표면 법선과 빛 방향이 얼마나 같은 쪽을 보는지 계산해 밝기를 정한다.

---

### 8. 외적: 두 방향에 수직인 축 만들기

외적은 두 벡터 모두에 수직인 새로운 벡터를 만든다.

```cpp
XMVECTOR perpendicular = XMVector3Cross(a, b);
```

대표적인 사용처는 다음과 같다.

```text
카메라의 Right 축 계산
삼각형 면의 Normal 계산
서로 수직인 좌표축 생성
회전축 계산
```

외적은 순서가 중요하다.

```text
A × B = -(B × A)
```

순서를 바꾸면 결과 방향이 반대가 된다. 이는 Left-Handed/Right-Handed 좌표계, 면의 앞뒤 방향 및 법선 방향과 연결된다.

---

### 9. 행렬은 좌표 변환 공식을 담은 표다

행렬을 단순히 숫자 16개로 외우기보다 다음처럼 이해하는 것이 좋다.

```text
행렬
→ 입력 좌표의 x, y, z, w를
→ 어떤 비율로 섞어서
→ 새로운 x, y, z, w를 만들지 나타내는 공식
```

대표적인 생성 함수는 다음과 같다.

```cpp
XMMatrixIdentity();               // 아무 변화 없음
XMMatrixTranslation(x, y, z);    // 이동
XMMatrixScaling(x, y, z);        // 크기 변경
XMMatrixRotationX(angle);         // X축 회전
XMMatrixRotationY(angle);         // Y축 회전
XMMatrixRotationZ(angle);         // Z축 회전
XMMatrixRotationRollPitchYaw(...);// 세 축 회전 결합
```

`XMMatrixIdentity()`는 곱해도 좌표가 변하지 않는 단위 행렬이다.

```text
Position × Identity = Position
```

따라서 변환이 없는 물체의 기본 World 행렬로 사용할 수 있다.

---

### 10. `XMMatrixRotationY()`가 회전을 만드는 원리

Y축 회전에서는 높이 `y`가 유지되고 `x`, `z`가 변한다.

```text
newX = x cosθ + z sinθ
newY = y
newZ = -x sinθ + z cosθ
```

DirectXMath의 행 벡터 규칙에서 Y축 회전 행렬은 개념적으로 다음 형태다.

```text
┌ cosθ   0  -sinθ  0 ┐
│  0     1    0    0 │
│ sinθ   0   cosθ  0 │
└  0     0    0    1 ┘
```

다음 함수가 `sinθ`, `cosθ`가 들어간 이 행렬을 만들어준다.

```cpp
XMMATRIX world = XMMatrixRotationY(rotation);
```

함수 호출만으로 Vertex Buffer의 데이터가 변경되는 것은 아니다. 행렬을 Object Constant Buffer로 보내고 Vertex Shader가 각 정점에 곱해야 화면의 정점 위치가 회전한다.

```hlsl
float4 worldPosition = mul(localPosition, World);
```

예를 들어 `(1, 0, 0)`을 90도 회전시키면 다음과 같다.

```text
θ = π/2
cosθ = 0
sinθ = 1

newX = 1×0 + 0×1 = 0
newY = 0
newZ = -1×1 + 0×0 = -1

(1, 0, 0) → (0, 0, -1)
```

매 프레임 회전 각도를 누적하면 연속적인 회전이 된다.

```cpp
rotation += deltaTime;
```

각도의 단위는 라디안이다.

```text
π 라디안 = 180도
π/2       = 90도
π/4       = 45도
1 라디안  ≈ 57.3도
```

자주 쓰는 상수와 함수는 다음과 같다.

```cpp
XM_PI
XM_2PI
XM_PIDIV2
XM_PIDIV4
XMConvertToRadians(degrees)
XMConvertToDegrees(radians)
```

`rotation += deltaTime`은 초당 약 1라디안, 즉 약 57.3도 회전한다는 뜻이다.

```cpp
rotation += rotationSpeed * deltaTime;
```

처럼 작성하면 초당 회전 속도를 분리할 수 있다.

---

### 11. World 행렬과 변환 순서

정점 배열에 들어 있는 좌표는 모델 자신의 원점을 기준으로 한 Local Space 좌표다.

```text
Local Space
→ World 행렬 적용
→ World Space
```

World 행렬은 일반적으로 크기, 회전, 이동을 결합한다. 현재처럼 행 벡터를 사용하는 규칙에서는 다음과 같은 순서를 사용할 수 있다.

```cpp
XMMATRIX world =
    XMMatrixScaling(scaleX, scaleY, scaleZ) *
    XMMatrixRotationRollPitchYaw(pitch, yaw, roll) *
    XMMatrixTranslation(positionX, positionY, positionZ);
```

이는 정점에 다음 순서로 적용된다.

```text
Local Position
→ Scale
→ Rotation
→ Translation
→ World Position
```

행렬 곱셈은 교환 법칙이 성립하지 않는다.

```text
A × B ≠ B × A
```

물체를 먼저 회전한 뒤 이동하면 물체가 자기 중심에서 회전한 후 해당 위치로 이동한다. 먼저 이동한 뒤 회전하면 이동된 위치까지 원점 주위로 회전하여 공전처럼 보일 수 있다.

```text
회전 → 이동
자전 후 배치

이동 → 회전
원점 주위 공전 가능
```

---

### 12. View 행렬과 카메라

View 행렬은 월드 좌표를 카메라 기준 좌표로 바꾼다.

```cpp
XMMATRIX view = XMMatrixLookAtLH(
    cameraPosition,
    cameraTarget,
    cameraUp
);
```

세 인수의 의미는 다음과 같다.

```text
cameraPosition
→ 카메라가 월드에서 존재하는 위치

cameraTarget
→ 카메라가 바라보는 월드의 한 점

cameraUp
→ 카메라 화면의 위쪽을 정하기 위한 기준 방향
```

Position과 Target만 있으면 바라보는 방향은 알 수 있지만, 카메라가 옆으로 얼마나 기울어졌는지는 알 수 없다. 같은 지점을 보면서도 카메라를 90도 굴릴 수 있기 때문에 Up 방향이 필요하다.

LookAt 행렬은 개념적으로 카메라의 세 축을 만든다.

```text
Forward = normalize(Target - Position)
Right   = normalize(cross(Up, Forward))
Up      = cross(Forward, Right)
```

View 행렬은 카메라 물체를 직접 화면에 그리는 행렬이 아니다. 카메라가 원점에 있고 기본 방향을 바라보는 것처럼 월드 전체를 반대로 변환한다.

```text
카메라를 오른쪽으로 이동
≈ 월드 전체를 왼쪽으로 이동해서 관찰
```

현재 프로젝트는 Left-Handed 함수인 `XMMatrixLookAtLH()`를 사용한다. 이 좌표계에서는 일반적으로 카메라의 전방을 `+Z`로 생각한다.

---

### 13. Projection 행렬과 원근감

View Space의 정점을 카메라 렌즈를 통과한 Clip Space 좌표로 바꾸는 것이 Projection 행렬이다.

```cpp
XMMATRIX projection = XMMatrixPerspectiveFovLH(
    XM_PIDIV4,
    1280.0f / 720.0f,
    0.1f,
    100.0f
);
```

각 인수는 다음 의미다.

```text
XM_PIDIV4
→ 세로 시야각 45도

1280 / 720
→ 화면 가로세로 비율

0.1
→ Near Plane, 카메라에서 가장 가까운 표시 거리

100.0
→ Far Plane, 카메라에서 가장 먼 표시 거리
```

함수는 각도를 도가 아니라 라디안으로 받는다. 따라서 `45.0f`를 직접 넣으면 안 된다.

```cpp
XM_PIDIV4
// 또는
XMConvertToRadians(45.0f)
```

Aspect Ratio가 실제 화면 비율과 다르면 원 또는 정육면체가 가로나 세로로 찌그러진다.

Projection 변환 후 GPU는 Perspective Divide를 수행한다.

```text
(x, y, z, w)
→ (x/w, y/w, z/w)
```

거리가 멀수록 Projection 결과의 `w`가 커지고, `x`와 `y`를 더 큰 값으로 나누므로 화면에서는 작게 보인다. 이것이 다른 프로젝트에서 크기를 직접 줄여 흉내 냈던 방식과 달리 실제 투영 행렬로 생기는 원근감이다.

Near/Far 값은 Depth Buffer의 정밀도에도 영향을 준다. Near를 지나치게 0에 가깝게 두거나 Far를 필요 이상으로 크게 잡으면 깊이 정밀도가 나빠져 겹친 표면이 깜빡이는 Z-fighting이 발생하기 쉬워진다.

---

### 14. 전체 좌표 변환 흐름

정점 하나가 화면 픽셀이 되기까지 공간은 다음 순서로 변한다.

```text
Local Space
모델 자체를 기준으로 저장된 정점
    ↓ World

World Space
게임 월드에 배치된 정점
    ↓ View

View Space
카메라를 원점으로 본 정점
    ↓ Projection

Clip Space
카메라 절두체를 기준으로 표현된 좌표
    ↓ Clipping과 Perspective Divide

NDC
화면과 독립적인 정규화 좌표
    ↓ Viewport Transform

Screen Space
실제 백 버퍼의 픽셀 위치
    ↓ Rasterization과 Depth Test

Render Target
최종 픽셀 색상
```

현재 HLSL은 이 흐름을 그대로 보여준다.

```hlsl
float4 localPosition = float4(input.position, 1.0f);
float4 worldPosition = mul(localPosition, World);
float4 viewPosition = mul(worldPosition, View);
output.position = mul(viewPosition, Projection);
```

Vertex Shader는 Projection 적용 후의 Clip Space 위치를 `SV_POSITION`으로 출력한다. 이후 Clipping, Perspective Divide, Viewport Transform은 고정 기능 파이프라인이 처리한다.

---

### 15. Index Buffer와 `DrawIndexed()`

Direct3D의 `TRIANGLELIST`는 인덱스 또는 정점을 세 개씩 읽어 삼각형 하나로 해석한다.

Index Buffer가 없으면 사각형 두 개를 만들 때 공유 정점을 중복 저장해야 한다.

```text
삼각형 1: A, B, C
삼각형 2: A, C, D

Vertex Buffer: A, B, C, A, C, D
```

Index Buffer를 사용하면 실제 정점은 한 번씩 저장하고 연결 순서만 따로 저장한다.

```text
Vertex Buffer: A, B, C, D
Index Buffer: 0, 1, 2, 0, 2, 3
```

현재 큐브는 위치만 고려했기 때문에 고유 정점 8개와 인덱스 36개를 사용한다.

```text
면 6개
× 면당 삼각형 2개
× 삼각형당 인덱스 3개
= 인덱스 36개
```

```cpp
DeviceContext->IASetIndexBuffer(
    IndexBuffer,
    DXGI_FORMAT_R32_UINT,
    0
);

DeviceContext->DrawIndexed(36, 0, 0);
```

`UINT`는 32비트이므로 `DXGI_FORMAT_R32_UINT`를 사용한다. `uint16_t`를 사용한다면 `DXGI_FORMAT_R16_UINT`를 사용해야 한다.

같은 위치에 있더라도 Normal이나 UV가 다르면 그래픽 파이프라인 관점에서 다른 정점이다. 텍스처와 면 단위 법선을 추가한 큐브는 일반적으로 정점 8개가 아니라 면당 4개씩 총 24개를 사용하고, 인덱스는 여전히 36개를 사용한다.

---

### 16. Winding Order는 삼각형마다 판정한다

GPU에는 사각형 또는 큐브 면이라는 기본 Primitive가 없다. 큐브의 한 면도 실제로는 삼각형 두 개다.

```cpp
0, 4, 5, // 삼각형 1
0, 5, 1  // 삼각형 2
```

앞면과 뒷면은 각 삼각형을 구성하는 세 인덱스의 순서로 결정된다.

```text
0, 4, 5
0, 5, 4

같은 위치의 삼각형이지만 방향은 반대
```

면을 배열에 기록하는 순서나 큐브의 여섯 면을 나열하는 순서는 일반적인 불투명 렌더링에서 중요하지 않다. 하지만 각 삼각형 내부의 인덱스 순서는 Back-face Culling과 법선 방향에 영향을 준다.

---

### 17. Depth Buffer가 필요한 이유

Back Buffer는 픽셀 색상을 저장하지만 어떤 픽셀이 카메라에서 더 가까운지는 기억하지 않는다. Depth Buffer는 화면의 각 픽셀마다 현재 가장 가까운 깊이를 저장한다.

```text
Back Buffer[x, y]
→ 현재 픽셀의 RGBA 색상

Depth Buffer[x, y]
→ 현재 픽셀에서 가장 가까운 깊이
```

프레임 시작 시 깊이를 가장 먼 값인 1.0으로 초기화한다.

```cpp
DeviceContext->ClearDepthStencilView(
    DSV,
    D3D11_CLEAR_DEPTH,
    1.0f,
    0
);
```

새로운 픽셀 후보가 기존 값보다 가까우면 색상과 깊이를 기록하고, 더 멀면 버린다.

```text
새 깊이 0.3, 기존 깊이 1.0
→ 통과, 0.3 기록

새 깊이 0.8, 기존 깊이 0.3
→ 실패, 가려진 픽셀
```

Depth Buffer도 GPU가 사용하는 2차원 데이터이므로 `ID3D11Texture2D`로 생성한다.

```text
Depth Texture
→ 실제 깊이 데이터 저장 공간

Depth Stencil View
→ 해당 Texture를 깊이/스텐실 대상으로 사용하는 View
```

```cpp
Device->CreateTexture2D(&depthDesc, nullptr, &DepthBuffer);
Device->CreateDepthStencilView(DepthBuffer, nullptr, &DSV);
DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
```

사용한 포맷은 다음과 같다.

```cpp
DXGI_FORMAT_D24_UNORM_S8_UINT
```

```text
24비트 → Depth
8비트  → Stencil
```

현재는 Depth만 사용한다. Stencil은 거울, 외곽선, 포털 및 특정 픽셀 영역 마스킹 같은 기능에 사용할 수 있다.

OpenGL에서도 Depth Buffer는 존재한다. 기본 프레임버퍼 또는 프레임워크가 생성 과정을 감췄을 수 있다.

```cpp
glEnable(GL_DEPTH_TEST);
glClear(GL_DEPTH_BUFFER_BIT);
```

Direct3D 11에서는 Depth Texture와 DSV를 직접 생성하고 연결했기 때문에 내부 구조가 코드에 더 명확하게 나타난 것이다.

---

### 18. Camera Buffer와 Object Buffer를 분리한 이유

현재 Constant Buffer 슬롯은 다음 규칙을 사용한다.

```text
b0 → CameraBuffer: View, Projection
b1 → ObjectBuffer: World
```

HLSL 선언은 다음과 같다.

```hlsl
cbuffer CameraBuffer : register(b0)
{
    matrix View;
    matrix Projection;
};

cbuffer ObjectBuffer : register(b1)
{
    matrix World;
};
```

Camera 데이터는 일반적으로 프레임 기준이고 World 데이터는 오브젝트 기준이다.

```text
View
→ 카메라가 움직이거나 회전할 때 변경

Projection
→ 창 크기, FOV, Near/Far가 바뀔 때 변경

World
→ 각 오브젝트의 위치, 회전, 크기가 바뀔 때 변경
```

오브젝트가 여러 개라면 Camera Buffer는 프레임당 한 번 갱신하고, Object Buffer는 각 오브젝트를 Draw하기 전에 갱신할 수 있다.

```text
Camera Buffer 갱신

Object A World 갱신 → Draw A
Object B World 갱신 → Draw B
Object C World 갱신 → Draw C
```

현재는 학습을 단순하게 유지하기 위해 View와 Projection을 같은 Dynamic Buffer에 담고 매 프레임 갱신한다. 이후 엔진 구조에서는 변경 빈도에 따라 더 세밀하게 최적화할 수 있다.

---

### 19. `DYNAMIC + Map/Unmap`을 선택한 이유

리소스 사용 방식은 데이터 변경 빈도에 따라 선택한다.

```text
IMMUTABLE
→ 생성 후 변경하지 않는 정점/인덱스 데이터

DEFAULT + UpdateSubresource
→ GPU 중심으로 사용하며 가끔 CPU에서 갱신하는 데이터

DYNAMIC + Map/Unmap
→ CPU가 자주 새 데이터를 작성하는 버퍼
```

Camera와 움직이는 Object의 행렬은 자주 변경되므로 다음 설정을 사용했다.

```cpp
desc.Usage = D3D11_USAGE_DYNAMIC;
desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
```

`Map()`은 CPU가 쓸 수 있는 메모리 주소를 얻는다.

```cpp
D3D11_MAPPED_SUBRESOURCE mapped = {};

HRESULT result = DeviceContext->Map(
    CameraBuffer,
    0,
    D3D11_MAP_WRITE_DISCARD,
    0,
    &mapped
);
```

`D3D11_MAP_WRITE_DISCARD`는 이전 내용이 필요 없으니 버리고 새로운 전체 내용을 작성하겠다는 뜻이다. GPU가 이전 메모리를 사용 중이라면 드라이버가 다른 메모리 영역을 제공하는 Resource Renaming을 수행해 CPU와 GPU가 서로 기다리는 상황을 줄일 수 있다.

`pData`는 자료형 정보가 없는 `void*`이므로 우리가 만든 구조체 포인터로 해석한다.

```cpp
CameraBufferData* data =
    static_cast<CameraBufferData*>(mapped.pData);
```

행렬을 직접 저장한 후 반드시 `Unmap()`한다.

```cpp
XMStoreFloat4x4(&data->View, XMMatrixTranspose(view));
XMStoreFloat4x4(&data->Projection, XMMatrixTranspose(projection));

DeviceContext->Unmap(CameraBuffer, 0);
```

`Unmap()` 이후에는 이전 `pData` 주소에 다시 접근하면 안 된다. 다음 `Map()`에서 다른 주소를 받을 수도 있다.

데이터 작성은 구조체 포인터 대입 또는 `memcpy()` 모두 가능하다.

```text
구조체 포인터 방식
→ 필드가 무엇인지 코드에서 바로 보임
→ 현재 행렬 저장 방식에 적합

memcpy 방식
→ 준비된 구조체 전체를 바이트 단위로 복사
→ 범용적이고 명확한 전체 복사
```

`Map()`의 반환값은 반드시 검사해야 한다. 실패한 `Map()`의 `pData`는 사용하면 안 된다.

---

### 20. Constant Buffer 슬롯 연결

두 버퍼를 HLSL의 연속된 `b0`, `b1` 슬롯에 한 번에 연결한다.

```cpp
ID3D11Buffer* constantBuffers[] =
{
    CameraBuffer,
    ObjectBuffer
};

DeviceContext->VSSetConstantBuffers(
    0,
    2,
    constantBuffers
);
```

인수의 의미는 다음과 같다.

```text
0
→ b0부터 연결

2
→ 버퍼 두 개 연결

constantBuffers
→ b0에 CameraBuffer, b1에 ObjectBuffer
```

HLSL의 `register(b0)`, `register(b1)`과 C++ 슬롯 번호가 일치해야 한다.

---

### 21. 행렬 전치와 OpenGL에서 전치하지 않았던 이유

현재 HLSL은 다음처럼 벡터를 왼쪽에 둔다.

```hlsl
mul(localPosition, World)
```

```text
행 벡터 × 행렬
```

DirectXMath의 일반적인 행 벡터 계산 규칙과 맞지만, HLSL의 `matrix`는 기본적으로 column-major 메모리 배치를 사용한다. 현재 코드는 CPU와 HLSL 사이의 메모리 해석을 맞추기 위해 전송 전에 전치한다.

```cpp
XMStoreFloat4x4(
    &data->World,
    XMMatrixTranspose(world)
);
```

여기서 구분해야 할 개념은 두 가지다.

```text
행 벡터/열 벡터
→ 수학적으로 벡터를 행렬의 어느 쪽에 두고 계산하는가

row-major/column-major
→ 행렬 숫자를 메모리에 어떤 순서로 저장하는가
```

OpenGL에서 흔히 사용하는 GLM과 GLSL 조합은 보통 다음 형태다.

```glsl
gl_Position = Projection * View * Model * position;
```

```text
행렬 × 열 벡터
```

GLM과 GLSL의 기본 규칙이 서로 맞기 때문에 보통 `GL_FALSE`로 전치 없이 전달한다.

```cpp
glUniformMatrix4fv(location, 1, GL_FALSE, value);
```

DirectX에서도 전치가 무조건 필수인 것은 아니다. HLSL에 `row_major`를 명시하는 설계도 가능하다.

```hlsl
row_major matrix World;
```

중요한 것은 한 가지 규칙을 프로젝트 전체에서 일관되게 사용하는 것이다. 현재 LightSaver는 다음 규칙으로 통일한다.

```text
HLSL: mul(vector, matrix)
CPU 전송: XMMatrixTranspose(matrix)
변환 순서: Local × World × View × Projection
```

---

### 22. 자주 사용하는 DirectXMath 함수

#### 벡터 생성과 기본 연산

```cpp
XMVectorSet(x, y, z, w)
// 네 성분으로 계산용 벡터 생성

XMVectorZero()
// 모든 성분이 0인 벡터

XMVectorAdd(a, b)
XMVectorSubtract(a, b)
XMVectorMultiply(a, b)
XMVectorScale(v, scalar)
// 덧셈, 뺄셈, 성분별 곱셈, 스칼라 곱
```

#### 벡터 길이와 방향

```cpp
XMVector3Length(v)
XMVector3LengthSq(v)
XMVector3Normalize(v)
XMVector3Dot(a, b)
XMVector3Cross(a, b)
```

#### 보간

```cpp
XMVectorLerp(a, b, t)
```

```text
t = 0 → a
t = 1 → b
t = 0.5 → a와 b의 중간
```

카메라 위치, 색상, UI 값 및 부드러운 이동에 사용할 수 있다. 프레임마다 고정 비율을 적용하는 단순 Lerp는 프레임레이트와 목표 도달 특성을 이해하고 사용해야 한다.

#### 벡터 변환

```cpp
XMVector3TransformCoord(position, matrix)
// 위치 변환에 사용, 이동 및 동차 좌표 처리 포함

XMVector3TransformNormal(direction, matrix)
// 방향/법선 변환에 사용, 이동 성분 제외
```

#### 행렬 생성

```cpp
XMMatrixIdentity()
XMMatrixTranslation(x, y, z)
XMMatrixScaling(x, y, z)
XMMatrixRotationX(angle)
XMMatrixRotationY(angle)
XMMatrixRotationZ(angle)
XMMatrixRotationRollPitchYaw(pitch, yaw, roll)
```

#### 카메라와 투영

```cpp
XMMatrixLookAtLH(position, target, up)
XMMatrixLookToLH(position, direction, up)
XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ)
XMMatrixOrthographicLH(width, height, nearZ, farZ)
```

`LookAt`은 목표 위치를 받고, `LookTo`는 바라볼 방향을 받는다. 1인칭 카메라에서는 위치와 Forward 방향을 이미 관리하므로 `LookToLH()`가 더 자연스러울 수 있다.

Perspective Projection은 멀리 있는 물체를 작게 보이게 한다. Orthographic Projection은 거리에 따라 크기가 변하지 않아 UI, 에디터 그리드, 미니맵 및 일부 그림자 맵에 사용할 수 있다.

#### 행렬 보조 함수

```cpp
XMMatrixTranspose(matrix)
// 행과 열 교환

XMMatrixInverse(determinant, matrix)
// 변환의 반대 효과를 내는 역행렬

XMMatrixMultiply(a, b)
// a * b와 같은 행렬 결합

XMMatrixDecompose(scale, rotation, translation, matrix)
// 행렬에서 크기, 회전, 이동 분리
```

역행렬은 World에서 Local로 되돌리거나, 카메라의 World Transform에서 View 행렬을 얻거나, 좌표계 사이를 역변환할 때 사용한다.

---

### 23. 이후 필요한 수학 원리

#### 역전치 행렬과 법선

물체에 균일하지 않은 크기 변환이 적용되면 법선을 World 행렬로 그대로 변환할 경우 표면에 수직인 성질이 깨질 수 있다. 이때 World 행렬의 역행렬을 구한 뒤 전치한 Normal Matrix를 사용한다.

```text
Normal Matrix = transpose(inverse(World))
```

텍스처와 조명을 추가할 때 자세히 적용한다.

#### Euler Angle과 Gimbal Lock

Pitch, Yaw, Roll 세 각도로 회전을 표현하면 직관적이지만 특정 축이 겹쳐 회전 자유도 하나를 잃는 Gimbal Lock이 생길 수 있다.

```cpp
XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
```

단순 FPS 카메라는 Yaw와 Pitch를 별도 값으로 관리하고 Pitch 범위를 제한하는 방식으로 충분할 수 있다. 복잡한 3D 회전 보간은 Quaternion이 더 적합하다.

#### Quaternion

자주 사용할 함수는 다음과 같다.

```cpp
XMQuaternionRotationRollPitchYaw(pitch, yaw, roll)
XMQuaternionNormalize(quaternion)
XMQuaternionSlerp(a, b, t)
XMMatrixRotationQuaternion(quaternion)
```

Quaternion은 4차원 벡터처럼 저장되지만 위치의 동차 좌표가 아니라 회전을 표현하는 별도의 수학 구조다. 회전 조합과 부드러운 보간에 유리하며 일반적인 Euler Angle 보간 문제를 줄인다.

#### 선형 보간과 구면 선형 보간

```text
Lerp
→ 직선상에서 값 보간

Slerp
→ 구면을 따라 회전 방향 보간
```

카메라 흔들림 복구, 몬스터 방향 전환, 문 회전 및 애니메이션 회전 보간 등에 사용할 수 있다.

---

### 24. 오늘 발견하고 수정한 오류

#### Projection FOV에 도 단위를 직접 전달함

잘못된 코드:

```cpp
XMMatrixPerspectiveFovLH(45.0f, ...);
```

DirectXMath 회전 함수는 라디안을 받으므로 다음처럼 수정했다.

```cpp
XMMatrixPerspectiveFovLH(XM_PIDIV4, ...);
```

#### Object Buffer를 Map한 뒤 Camera Buffer를 Unmap함

잘못된 코드:

```cpp
DeviceContext->Map(ObjectBuffer, ...);
DeviceContext->Unmap(CameraBuffer, 0);
```

Map한 리소스와 같은 리소스를 Unmap해야 한다.

```cpp
DeviceContext->Map(ObjectBuffer, ...);
DeviceContext->Unmap(ObjectBuffer, 0);
```

#### 새 COM 리소스의 Release 누락

추가된 리소스도 종료할 때 해제해야 한다.

```text
ObjectBuffer
CameraBuffer
IndexBuffer
InputLayout
Pixel Shader
Vertex Shader
DSV
DepthBuffer
```

#### D3D10 이름 사용

```cpp
D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST
```

동일 계열 값이 호환되더라도 Direct3D 11 프로젝트이므로 다음 이름으로 통일했다.

```cpp
D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
```

---

### 25. 현재 프레임의 실제 실행 순서

```text
1. QueryPerformanceCounter로 deltaTime 계산

2. rotation += deltaTime

3. XMMatrixRotationY로 Object의 World 행렬 생성

4. CameraBuffer Map
   → View 전치 후 저장
   → Projection 전치 후 저장
   → CameraBuffer Unmap

5. ObjectBuffer Map
   → World 전치 후 저장
   → ObjectBuffer Unmap

6. RTV와 DSV를 Output Merger에 연결

7. Back Buffer 색상 초기화

8. Depth Buffer를 1.0으로 초기화

9. Viewport 설정

10. Input Layout, Vertex Buffer, Index Buffer, Topology 설정

11. CameraBuffer를 b0, ObjectBuffer를 b1에 연결

12. Vertex Shader와 Pixel Shader 연결

13. DrawIndexed(36, 0, 0)

14. Present(1, 0)
```

---

### 오늘의 핵심 요약

```text
DirectXMath는 CPU 벡터/행렬 계산 라이브러리다.
Direct3D 11은 GPU 리소스와 렌더링 파이프라인을 제어하는 API다.
XMFLOAT 계열은 저장용이고 XMVECTOR/XMMATRIX는 SIMD 계산용이다.
w = 1은 위치, w = 0은 방향을 표현한다.
World는 Local 좌표를 게임 월드에 배치한다.
View는 월드를 카메라 기준으로 변환한다.
Projection은 카메라 공간에 원근감과 가시 범위를 적용한다.
정점은 Local → World → View → Projection 순서로 변환된다.
Index Buffer는 실제 정점 대신 사용할 정점 번호와 연결 순서를 저장한다.
Depth Buffer는 픽셀마다 가장 가까운 깊이를 저장한다.
Camera Buffer와 Object Buffer는 데이터의 변경 단위가 달라 분리한다.
DYNAMIC Constant Buffer는 Map/Unmap과 WRITE_DISCARD로 자주 갱신할 수 있다.
현재 행렬 규칙은 HLSL의 mul(vector, matrix)와 CPU의 Transpose 전송이다.
회전 행렬은 sin과 cos로 각 정점의 축 성분을 다시 계산한다.
행렬 곱셈 순서는 결과에 직접 영향을 주므로 프로젝트 전체에서 일관되게 유지해야 한다.
```

---

## 2026-08-14 — Window, Timer, Graphics, GameLoop 분리와 실행 경로 재구성

### 1. 이번 구조 변경의 목적

기존 프로그램은 `WinMain` 하나가 다음 역할을 모두 담당했다.

```text
Win32 창 생성
메시지 처리
시간 계산
Direct3D 11 초기화
큐브 리소스 생성
게임 상태 갱신
렌더링
COM 리소스 해제
```

큐브 하나를 출력하는 단계에서는 실행 순서를 한 파일에서 확인하기 쉽다는 장점이 있다. 하지만 오브젝트, 입력, 카메라, 조명과 게임 규칙이 추가되면 `WinMain`이 모든 시스템의 세부 구현을 알아야 한다. 이번 작업에서는 렌더링 결과를 바꾸는 대신 프로그램의 수명과 매 프레임 실행 순서를 클래스로 분리했다.

현재 실행 구조는 다음과 같다.

```text
WinMain
└─ LightSaverGame : public GameLoop
   ├─ GameLoop
   │  ├─ Windows
   │  ├─ Timer
   │  └─ Graphics
   │
   └─ LightSaverGame
      ├─ 큐브 GPU 리소스
      ├─ OnInitialize
      ├─ Update
      └─ Render
```

`LightSaverGame`이 `GameLoop` 객체를 멤버로 가지는 것이 아니라 `GameLoop`를 상속한다. `GameLoop`는 실행 순서를 제공하고, `LightSaverGame`은 게임마다 달라지는 초기화·업데이트·렌더링 동작을 구현한다.

---

### 2. WinMain의 최종 역할

현재 `WinMain`은 다음 세 가지 일만 한다.

```text
1. LightSaverGame 객체 생성
2. Initialize 호출
3. Run 호출 후 반환
```

프로그램 진입점은 더 이상 창, D3D11 버퍼, 셰이더와 메시지 루프의 세부 구현을 알지 않는다. 실행 흐름은 다음과 같다.

```text
Windows가 WinMain 호출
        ↓
LightSaverGame 생성
        ↓
GameLoop::Initialize
        ↓
GameLoop::Run
        ↓
WinMain에 종료 코드 반환
```

기존 큐브 코드는 이전 동작과 새 구조를 비교할 수 있도록 `#if 0`과 `#endif` 사이에 임시 보관했다. 전처리기의 조건이 `0`, 즉 거짓이므로 그 구간은 컴파일 전에 제거된다. 새 구조의 이전이 끝난 뒤에는 Git 기록을 통해 복구할 수 있으므로 제거할 수 있다.

---

### 3. GameLoop가 담당하는 실행 순서

`GameLoop`는 게임마다 반복되는 공통 실행 골격을 가진다.

```text
GameLoop::Initialize
├─ Windows::Initialize
├─ Graphics::Initialize
├─ LightSaverGame::OnInitialize
└─ initialized = true

GameLoop::Run
└─ while (isRunning)
   ├─ Windows::PeekMSG
   ├─ Timer::GetDeltaTime
   ├─ LightSaverGame::Update
   ├─ LightSaverGame::Render
   └─ SwapChain::Present
```

`OnInitialize`, `Update`, `Render`는 가상 함수다. `GameLoop` 코드 안에서 호출하지만 실제 객체가 `LightSaverGame`이므로 런타임에는 `LightSaverGame`이 재정의한 함수가 호출된다. 이것이 다형성을 사용해 공통 루프와 게임별 동작을 분리한 부분이다.

```cpp
virtual bool OnInitialize() = 0;
virtual void Update(float deltaTime) = 0;
virtual bool Render() = 0;
```

순수 가상 함수 `= 0`을 가진 `GameLoop`는 그 자체로 실행할 수 없는 추상 클래스다. 반드시 이를 상속하고 세 함수를 구현한 구체 클래스가 필요하다.

---

### 4. initialized와 isRunning의 차이

두 변수는 서로 다른 상태를 나타낸다.

```text
initialized
└─ Window, Graphics, 게임 리소스 초기화가 전부 성공했는가?

isRunning
└─ 초기화가 끝난 프로그램이 현재 반복 실행 중인가?
```

`Run`이 초기화 전에 호출되면 필요한 Window, SwapChain과 Device가 없으므로 접근 오류가 발생할 수 있다. `initialized`는 이 잘못된 호출 순서를 차단한다.

`isRunning`은 실행 중 `RequestExit()`가 호출되었을 때 루프를 끝내기 위한 상태다. 따라서 초기화 성공 여부와 현재 실행 여부는 같은 `bool` 하나로 합치지 않고 분리한다.

---

### 5. Windows, Timer, Graphics의 소유 관계

`GameLoop`가 세 시스템을 값 멤버로 소유한다.

```cpp
Windows windows;
Graphics graphics;
Timer timer;
```

전역 변수나 Singleton을 사용하지 않아도 `GameLoop`의 수명 동안 세 객체가 함께 존재한다. `LightSaverGame`은 보호 함수 `GetGraphics()`를 통해 이미 생성된 `Graphics`에 접근한다.

```text
GetGraphics()가 하는 일
└─ 새 Graphics 객체 생성 X
   GameLoop가 가진 기존 Graphics 객체의 참조 반환 O
```

복사를 막기 위해 `GameLoop`의 복사 생성자와 복사 대입 연산자는 삭제했다. Window 핸들과 COM 포인터를 가진 객체가 얕게 복사되면 같은 운영체제/GPU 리소스를 여러 객체가 해제할 위험이 있기 때문이다.

---

### 6. LightSaverGame의 현재 책임은 임시 단계다

현재 `LightSaverGame`은 큐브 하나를 새 게임 루프에서 다시 출력하기 위해 다음 리소스를 직접 가진다.

```text
VertexBuffer
IndexBuffer
VertexShader
PixelShader
InputLayout
CameraBuffer
ObjectBuffer
Viewport
Rotation
ClearColor
```

이 구조는 최종 구조가 아니다. 기존 `WinMain`에서 기능을 안전하게 옮기고 실행 결과가 동일한지 확인하는 중간 단계다. 이후 분리 방향은 다음과 같다.

```text
VertexBuffer + IndexBuffer      → Mesh
VS + PS + InputLayout           → Shader
Shader + Texture + 재질 값       → Material
Mesh와 Material 참조             → MeshComponent
Clear/바인딩/DrawIndexed         → Renderer
Rotation과 World Transform       → Actor/SceneComponent
Camera View/Projection           → CameraComponent와 Renderer
```

두 몬스터가 같은 모델을 사용할 경우 각 Actor가 GPU 버퍼를 따로 만드는 것이 아니라 같은 `Mesh`를 참조하고 서로 다른 World 행렬로 그리게 된다.

---

### 7. OnInitialize에서 생성되는 리소스

현재 `LightSaverGame::OnInitialize`의 작업 순서는 다음과 같다.

```text
큐브 정점 배열 준비
→ Vertex Buffer 생성
→ 큐브 인덱스 배열 준비
→ Index Buffer 생성
→ HLSL Vertex/Pixel Shader 컴파일
→ Vertex Shader와 Pixel Shader 생성
→ Input Layout 생성
→ 임시 Shader Blob 해제
→ Viewport 설정
→ Camera/Object Constant Buffer 생성
→ 움직이는 카메라가 사용할 GPU 공간 준비
```

정점과 인덱스 배열, 각종 `D3D11_*_DESC`, `HRESULT`, Shader Blob은 초기화 함수 안에서만 필요하므로 지역변수다. 반대로 생성된 GPU 리소스는 이후 프레임에서도 사용하므로 `LightSaverGame`의 멤버다.

```text
지역변수
└─ 함수가 끝나면 필요 없음

멤버변수
└─ OnInitialize에서 생성한 뒤 Render와 소멸자에서도 사용
```

---

### 8. 움직이는 CameraBuffer를 Render에서 매 프레임 기록하는 이유

다음 호출은 GPU 버퍼 공간을 만들지만 초기 데이터는 전달하지 않는다.

```cpp
CreateBuffer(&CameraDesc, nullptr, &CameraBuffer);
```

두 번째 인수가 `nullptr`이므로 View와 Projection은 아직 들어 있지 않다. 고정 카메라 단계에서는 생성 직후 한 번 기록해도 충분했지만, 현재는 WASD와 마우스로 카메라가 움직이므로 `Render()`에서 매 프레임 다음 순서로 기록한다.

```text
Map(CameraBuffer, WRITE_DISCARD)
→ CPU가 쓸 주소 획득
→ View와 Projection 전치 후 저장
→ Unmap(CameraBuffer)
```

현재 Projection은 고정되어 있지만 View가 매 프레임 바뀐다. 두 행렬이 같은 `CameraBuffer`에 들어 있으므로 함께 기록한다. 나중에 Renderer를 분리하면 Camera는 CPU 상태와 행렬 계산을 담당하고 Renderer가 계산 결과를 GPU 버퍼에 업로드한다.

---

### 9. Update와 Render의 책임 분리

현재 `Update`는 CPU 게임 상태와 카메라 상태를 바꾼다.

```cpp
Rotation += deltaTime;
키보드 입력으로 Camera::Position 변경;
마우스 이동량으로 Camera::Yaw/Pitch 변경;
```

`Render`는 회전 상태로 World 행렬을 계산하고 GPU ObjectBuffer를 갱신한 뒤 실제 그리기 명령을 실행한다.

```text
Update
├─ Rotation 변경
├─ WASD 입력 결합과 대각선 속도 보정
├─ Camera Position 변경
└─ 마우스 이동량으로 Yaw/Pitch 변경

Render
├─ XMMatrixRotationY로 World 계산
├─ Camera가 View/Projection 계산
├─ CameraBuffer Map/Unmap
├─ ObjectBuffer Map/Unmap
├─ RenderTarget/DepthStencil 설정 및 Clear
├─ Viewport 설정
├─ Input Assembler 설정
├─ Constant Buffer와 Shader 바인딩
└─ DrawIndexed(36, 0, 0)
```

CameraBuffer와 ObjectBuffer 갱신을 `Render`에 둔 이유는 `Map/Unmap`이 GPU에 전달할 렌더링 데이터를 준비하는 작업이기 때문이다. 또한 `Render`는 `bool`을 반환하므로 `Map` 실패를 `GameLoop`에 전달할 수 있다. `void Update`에서 단순히 `return`하면 렌더링이 계속되어 이전 프레임의 데이터로 그려질 수 있다.

---

### 10. Present는 GameLoop에서 한 번만 호출한다

`LightSaverGame::Render`는 Back Buffer에 그리기만 하고 화면 교체는 하지 않는다.

```text
LightSaverGame::Render
└─ DrawIndexed까지 수행

GameLoop::Run
└─ Render 성공 후 Present(1, 0)
```

게임별 Render마다 `Present`를 호출하면 공통 루프가 프레임 종료와 오류 처리를 제어할 수 없다. Present를 `GameLoop` 한 곳에 두면 한 프레임에 정확히 한 번 호출된다는 규칙이 생긴다.

`Present(1, 0)`의 첫 번째 인수 `1`은 수직 동기화를 사용해 다음 화면 갱신 시점에 표시하라는 뜻이다.

---

### 11. COM 리소스 수명과 소멸 순서

`LightSaverGame`은 자신이 생성한 COM 리소스를 소멸자에서 해제한다.

```text
ObjectBuffer
CameraBuffer
IndexBuffer
VertexBuffer
InputLayout
PixelShader
VertexShader
```

초기화가 중간에 실패할 수 있으므로 모든 포인터는 `nullptr`로 시작한다. 소멸자는 각 포인터가 `nullptr`이 아닐 때만 `Release()`한다. 그러면 생성에 성공한 리소스만 안전하게 해제된다.

상속 객체가 파괴될 때는 파생 클래스 부분이 먼저 파괴되고 그다음 기반 클래스가 파괴된다.

```text
LightSaverGame 소멸자
→ 큐브의 Buffer와 Shader Release
→ GameLoop 소멸
→ GameLoop가 가진 Graphics 소멸
→ DeviceContext, SwapChain, Device 등 Release
```

따라서 큐브 GPU 리소스는 그것을 만든 Graphics Device보다 먼저 해제된다.

---

### 12. FOV는 라디안 단위다

`XMMatrixPerspectiveFovLH`의 첫 번째 인수는 도(degree)가 아니라 라디안이다.

```cpp
DirectX::XMMatrixPerspectiveFovLH(
    DirectX::XM_PIDIV4,
    1280.f / 720.f,
    0.1f,
    100.f);
```

`XM_PIDIV4`는 π/4 라디안이며 45도와 같다. `45.f`를 직접 전달하면 45도가 아니라 45라디안으로 해석된다. 실수 타입 자체는 올바르므로 컴파일러는 오류를 내지 않지만 투영 결과는 잘못된다. 이것은 문법 오류가 아니라 API 단위 계약을 어긴 의미상 오류다.

---

### 13. 현재 단계에서 얻은 구조적 기준

```text
WinMain은 조립과 실행만 담당한다.
GameLoop는 매 프레임 실행 순서를 소유한다.
Windows는 Win32 창과 메시지를 담당한다.
Timer는 프레임 간 시간 차이를 계산한다.
Graphics는 Direct3D Device/Context/SwapChain과 기본 Target을 소유한다.
LightSaverGame은 현재 게임별 초기화·상태·렌더링을 구현한다.
Update는 게임 상태를 변경한다.
Render는 GPU 데이터 갱신과 그리기 명령을 담당한다.
Present는 공통 GameLoop에서 한 번만 호출한다.
COM 리소스는 소유한 객체가 해제한다.
```

다음 구조 단계에서는 먼저 `Shader`를 만들어 VS/PS/Input Layout과 컴파일 코드를 분리하고, 이어서 `Mesh`에 Vertex/Index Buffer의 소유권과 생성 코드를 옮긴다. 이후 Material, MeshComponent, Renderer, World, Actor 순으로 확장한다.

---

## 2026-08-14 — FPS 카메라 수학, WASD 이동과 마우스 회전

### 1. 이번 단계의 목표와 결과

이전에는 카메라 위치와 바라보는 지점을 초기화 코드에 직접 적었다.

```cpp
XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
```

이 방식은 고정된 큐브를 확인하기에는 충분하지만 플레이어가 움직이는 FPS 카메라에는 부족하다. 이번 단계에서는 카메라의 상태를 `Camera` 클래스로 분리하고 다음 흐름을 구현했다.

```text
WASD 키 상태
→ 앞/오른쪽 입력 축 계산
→ 대각선 속도 보정
→ Camera Position 변경

마우스의 화면상 이동량
→ 픽셀당 라디안 감도 적용
→ Camera Yaw/Pitch 변경

Position + Yaw + Pitch
→ Forward/Right/Up 계산
→ View 행렬 계산
→ CameraBuffer에 View/Projection 업로드
→ 움직인 카메라 기준으로 큐브 렌더링
```

현재 `Camera`가 직접 저장하는 값은 다음과 같다.

```cpp
XMFLOAT3 Position;
float Yaw;
float Pitch;
float FovY;
float AspectRatio;
float NearZ;
float FarZ;
```

`Forward`, `Right`, `Up`, `View`, `Projection`은 따로 저장하지 않고 위 상태에서 계산한다. 계산 가능한 결과를 여러 곳에 중복 저장하면 한쪽만 갱신되어 서로 불일치할 수 있기 때문이다.

---

### 2. 현재 좌표계와 기준 방향

현재 코드는 이름이 `LH`로 끝나는 DirectXMath 함수를 사용한다.

```text
+X = 오른쪽
+Y = 위쪽
+Z = 앞쪽
```

카메라가 회전하지 않은 초기 상태의 기준 Forward는 다음과 같다.

```text
BaseForward = (0, 0, 1)
```

`Yaw = 0`, `Pitch = 0`을 Forward 공식에 넣어도 반드시 `(0, 0, 1)`이 나와야 한다. 이것은 공식을 검사하는 가장 쉬운 기준이다.

`WorldUp`은 월드 전체에서 변하지 않는 위쪽 축이다.

```text
WorldUp = (0, 1, 0)
```

카메라가 위를 보더라도 `WorldUp`이 기울어지는 것은 아니다. 카메라가 회전한 뒤의 위쪽은 `CameraUp`이며, 월드의 고정된 위쪽인 `WorldUp`과 서로 다른 개념이다.

---

### 3. 도와 라디안

사람은 보통 45도, 90도처럼 도 단위를 사용하지만 C++의 삼각함수와 DirectXMath 회전 함수는 라디안을 사용한다.

```text
180 degree = π radian
90 degree  = π/2 radian
45 degree  = π/4 radian
```

변환식은 다음과 같다.

```text
radian = degree × π / 180
degree = radian × 180 / π
```

DirectXMath에서는 직접 π를 곱하지 않고 다음 함수를 사용할 수 있다.

```cpp
XMConvertToRadians(89.0f);
XMConvertToDegrees(angleInRadians);
```

`std::sin`, `std::cos`, `XMMatrixRotationY`, `XMMatrixPerspectiveFovLH`에 전달하는 각도도 라디안이다. 변수 타입이 모두 `float`이므로 도를 잘못 전달해도 컴파일 오류는 발생하지 않는다. 단위는 타입이 아니라 프로그래머가 지켜야 하는 계약이다.

---

### 4. Yaw와 Pitch가 의미하는 회전

FPS 카메라는 현재 Roll을 사용하지 않고 두 각도만 사용한다.

```text
Yaw   = 월드 Y축을 중심으로 좌우 회전
Pitch = 카메라가 위아래를 바라보는 회전
```

현재 기준에서 `Yaw = 0`이면 +Z를 보고, 양의 Yaw가 증가하면 +X 쪽으로 회전한다.

먼저 위아래를 보지 않는 수평면만 생각하면 단위원에서 다음 관계가 나온다.

```text
x = sin(Yaw)
z = cos(Yaw)
```

검산하면 다음과 같다.

```text
Yaw = 0°  → (x, z) = (0, 1)  → +Z
Yaw = 90° → (x, z) = (1, 0)  → +X
```

Pitch가 추가되면 수평 성분 전체의 크기가 `cos(Pitch)`로 줄고, 그만큼 수직 성분 `sin(Pitch)`가 생긴다.

```text
수평 길이 = cos(Pitch)
y         = sin(Pitch)
```

수평 길이를 Yaw에 따라 X와 Z로 나누면 최종 Forward 공식이 된다.

```text
x = cos(Pitch) × sin(Yaw)
y = sin(Pitch)
z = cos(Pitch) × cos(Yaw)
```

코드는 다음과 같다.

```cpp
XMVECTOR forward = XMVectorSet(
    std::cos(Pitch) * std::sin(Yaw),
    std::sin(Pitch),
    std::cos(Pitch) * std::cos(Yaw),
    0.0f);

return XMVector3Normalize(forward);
```

Pitch가 커질수록 Y가 증가하고 XZ 평면에 남는 길이가 줄어든다. 이것은 전체 길이를 일정하게 유지하면서 방향만 위쪽으로 기울이는 과정이다.

---

### 5. XMFLOAT3와 XMVECTOR의 역할 차이

`XMFLOAT3`와 `XMVECTOR`는 모두 숫자 여러 개를 담지만 목적이 다르다.

```text
XMFLOAT3
└─ 메모리에 오래 저장하기 좋은 x, y, z 구조체

XMVECTOR
└─ CPU SIMD 레지스터에서 계산하기 좋은 4개 성분 벡터
```

따라서 Camera Position은 `XMFLOAT3`로 저장하고 계산할 때 `XMVECTOR`로 불러온다.

```cpp
XMVECTOR position = XMLoadFloat3(&Position);
// 벡터 계산
XMStoreFloat3(&Position, position);
```

`XMVECTOR`는 네 성분을 가지지만 `XMVector3...` 함수는 이름처럼 주로 XYZ를 3차원 벡터로 해석한다. 방향 벡터의 W는 0으로 둔다.

```text
위치: (x, y, z, 1)
방향: (x, y, z, 0)
```

동차좌표 변환에서 위치는 이동 행렬의 영향을 받아야 하지만 방향은 받아서는 안 되기 때문이다. 현재 `XMVector3Normalize`는 길이를 계산할 때 XYZ를 사용한다. Forward의 W가 0이면 정규화 후에도 0으로 유지되어 방향이라는 의미가 보존된다.

---

### 6. 정규화가 필요한 이유

정규화는 벡터의 방향은 유지하고 길이를 1로 만드는 연산이다.

```text
normalize(v) = v / |v|
```

예를 들어 `(3, 0, 4)`의 길이는 5다.

```text
normalize(3, 0, 4)
= (3/5, 0, 4/5)
= (0.6, 0, 0.8)
```

방향 벡터의 길이가 1이어야 다음 계산이 직관적으로 유지된다.

```text
이동량 = 단위 방향 × 실제 이동 거리
```

DirectXMath에서는 다음 함수를 사용한다.

```cpp
XMVector3Normalize(vector);
```

이 함수는 원본 변수를 직접 고치지 않고 계산 결과를 반환한다. 따라서 반드시 반환값을 받아야 한다.

```cpp
forward = XMVector3Normalize(forward); // 올바름
XMVector3Normalize(forward);           // 결과를 버리므로 forward는 그대로
```

---

### 7. 외적으로 Right와 CameraUp 만들기

외적은 두 벡터가 만드는 평면에 수직인 벡터를 구한다. 결과 방향은 입력 순서에 따라 달라진다.

```text
A × B = -(B × A)
```

기본 축의 순환은 다음과 같이 기억할 수 있다.

```text
X × Y = Z
Y × Z = X
Z × X = Y
```

순서를 뒤집으면 음수가 된다.

```text
Y × X = -Z
Z × Y = -X
X × Z = -Y
```

현재 카메라의 Right는 다음 순서로 계산한다.

```cpp
Right = normalize(WorldUp × Forward);
```

카메라가 초기 Forward인 +Z를 볼 때 계산해 보면:

```text
WorldUp × Forward
= Y × Z
= X
= 오른쪽
```

일반적인 Forward를 `(Fx, Fy, Fz)`라고 하면 결과는 다음과 같다.

```text
(0, 1, 0) × (Fx, Fy, Fz)
= (Fz, 0, -Fx)
```

결과의 Y는 항상 0이다. 따라서 카메라가 위아래를 보더라도 Right는 지면과 평행하다. 여기서 WorldUp과 Forward가 서로 수직일 필요는 없다. 외적이 두 벡터 모두에 수직인 새로운 방향을 계산한다.

CameraUp은 Forward와 Right 양쪽에 수직이어야 한다.

```cpp
CameraUp = normalize(Forward × Right);
```

초기 방향으로 검산하면:

```text
Forward × Right
= Z × X
= Y
= 위쪽
```

최종적으로 카메라는 서로 직교하는 세 기준축을 얻는다.

```text
Right     = WorldUp × Forward
CameraUp  = Forward × Right
Forward   = Yaw/Pitch에서 계산
```

---

### 8. WorldUp과 CameraUp이 다른 이유

카메라가 위쪽을 보면 Forward에는 Y 성분이 생긴다.

```text
Forward ≈ (0, 0.707, 0.707)
```

그러나 월드의 위쪽은 계속 다음과 같다.

```text
WorldUp = (0, 1, 0)
```

WorldUp은 중력 방향과 지면의 기준을 정하는 월드 좌표축이다. 카메라가 어디를 본다고 월드 전체가 기울지는 않는다. 반면 CameraUp은 회전된 카메라 화면에서 위쪽이 어디인지 나타낸다. `XMMatrixLookToLH`에는 실제 카메라 자세를 만들기 위해 CameraUp을 전달한다.

이 방식은 FPS 카메라의 Roll을 0으로 유지한다. 비행기처럼 카메라를 옆으로 기울이는 Roll까지 구현한다면 WorldUp만으로 Right를 계속 재구성하는 방식보다 Quaternion 또는 카메라 자체 회전축을 관리하는 방식이 필요하다.

---

### 9. View 행렬: XMMatrixLookToLH

View 행렬은 카메라를 월드에 배치하는 행렬이 아니라, 월드 전체를 카메라 기준 좌표로 바꾸는 행렬이다.

```cpp
XMMatrixLookToLH(
    CameraPos,
    GetForwardVector(),
    GetUpVector());
```

각 인수의 의미는 다음과 같다.

```text
EyePosition  = 카메라의 월드 위치
EyeDirection = 카메라가 바라보는 방향
UpDirection  = 카메라 화면의 위쪽 방향
```

`LookAt`과 `LookTo`의 차이는 두 번째 인수다.

```text
XMMatrixLookAtLH(Position, TargetPosition, Up)
→ 월드의 어느 지점을 볼 것인지 전달

XMMatrixLookToLH(Position, ForwardDirection, Up)
→ 어느 방향을 볼 것인지 전달
```

현재 Camera는 Target을 저장하지 않고 Yaw/Pitch로 Forward를 계산하므로 `LookToLH`가 더 자연스럽다.

---

### 10. Projection 행렬과 카메라 렌즈 값

원근 투영은 다음 함수로 만든다.

```cpp
XMMatrixPerspectiveFovLH(FovY, AspectRatio, NearZ, FarZ);
```

각 값은 다음 의미를 가진다.

```text
FovY        = 세로 시야각, 라디안 단위
AspectRatio = 화면 너비 / 화면 높이
NearZ       = 카메라에서 가장 가까이 보이는 거리
FarZ        = 카메라에서 가장 멀리 보이는 거리
```

현재 값은 다음과 같다.

```cpp
FovY = XM_PIDIV4;          // 45도
AspectRatio = 1280 / 720; // 약 1.777, 16:9
NearZ = 0.1f;
FarZ = 100.0f;
```

AspectRatio가 맞지 않으면 화면이 가로 또는 세로로 찌그러진다. NearZ보다 가까운 물체와 FarZ보다 먼 물체는 클리핑된다. NearZ를 지나치게 0에 가깝게 두면 제한된 Depth Buffer 정밀도를 비효율적으로 사용해 Z-fighting이 심해질 수 있다.

---

### 11. 지면 이동에서 Forward의 Y를 제거하는 이유

카메라가 위를 볼 때 Forward에는 양의 Y가 들어간다. 이 Forward로 그대로 움직이면 W를 눌렀을 때 카메라가 공중으로 올라간다. 걷는 FPS 카메라는 시선과 이동을 분리해야 한다.

```cpp
XMVECTOR forward = GetForwardVector();
forward = XMVectorSetY(forward, 0.0f);
forward = XMVector3Normalize(forward);
```

Y를 0으로 만든 뒤 다시 정규화하는 이유가 중요하다. 예를 들어 45도 위를 볼 때 Forward는 대략 다음과 같다.

```text
(0, 0.707, 0.707)
```

Y만 제거하면:

```text
(0, 0, 0.707)
```

방향은 맞지만 길이가 0.707이므로 같은 이동 거리를 곱해도 느리게 움직인다. 다시 정규화하면 `(0, 0, 1)`이 되어 시선의 Pitch와 관계없이 지면 이동 속도가 일정해진다.

Right는 `WorldUp × Forward`로 만들 때 수식상 Y가 0이므로 현재 구조에서는 별도로 Y를 제거할 필요가 없다.

---

### 12. 위치 이동에 사용한 DirectXMath 함수

저장된 Position을 계산용 벡터로 불러온다.

```cpp
XMVECTOR newPos = XMLoadFloat3(&Position);
```

단위 방향에 실제 이동 거리를 곱한다.

```cpp
XMVECTOR move = XMVectorScale(direction, distance);
```

현재 위치와 이동량을 더한다.

```cpp
newPos = XMVectorAdd(newPos, move);
```

계산 결과를 저장용 구조체에 다시 기록한다.

```cpp
XMStoreFloat3(&Position, newPos);
```

전체 의미는 일반 벡터식과 같다.

```text
NewPosition = OldPosition + Direction × Distance
```

---

### 13. deltaTime과 프레임 독립적인 이동

키를 누르고 있는 동안 프레임마다 이동하므로 프레임 수가 많을수록 더 빨라지는 문제가 생길 수 있다.

```text
60 FPS에서 프레임당 1 이동  → 1초에 60 이동
144 FPS에서 프레임당 1 이동 → 1초에 144 이동
```

따라서 초당 속도에 이번 프레임이 차지한 시간을 곱한다.

```cpp
float MoveDistance = CameraSpeed * deltaTime;
```

단위로 확인하면 다음과 같다.

```text
CameraSpeed: 3 world-unit / second
deltaTime:   second / frame

3 world-unit/second × second/frame
= 3 × deltaTime world-unit/frame
```

그 결과 FPS가 달라도 같은 실제 시간 동안 이동한 총거리가 거의 같아진다.

---

### 14. WASD 입력과 0x8000

현재 키 상태는 Win32 함수로 확인한다.

```cpp
GetAsyncKeyState('W') & 0x8000
```

`GetAsyncKeyState`는 `SHORT` 비트값을 반환한다. 최상위 비트가 1이면 호출 시점에 키가 눌려 있다. `0x8000`은 16비트 값에서 그 최상위 비트만 켠 마스크다.

```text
0x8000 = 1000 0000 0000 0000₂
```

비트 AND 결과가 0이 아니면 현재 눌림 상태다.

입력은 즉시 이동시키지 않고 먼저 두 축에 모은다.

```text
ForwardInput: W = +1, S = -1
RightInput:   D = +1, A = -1
```

따라서 W와 S를 동시에 누르면 0, A와 D를 동시에 눌러도 0이 된다.

---

### 15. 대각선 이동이 빨라지는 이유와 해결

W만 누르면 입력 벡터 길이는 1이다.

```text
(Forward, Right) = (1, 0)
length = √(1² + 0²) = 1
```

W와 D를 동시에 누르면 두 축에 각각 1이 들어간다.

```text
(Forward, Right) = (1, 1)
length = √(1² + 1²) = √2 ≈ 1.414
```

보정하지 않으면 대각선 이동이 직선 이동보다 약 41.4% 빠르다. 현재 코드는 입력 길이가 1보다 클 때 두 축을 그 길이로 나눈다.

```cpp
float inputLength = std::sqrt(
    ForwardInput * ForwardInput +
    RightInput * RightInput);

if (inputLength > 1.0f)
{
    ForwardInput /= inputLength;
    RightInput /= inputLength;
}
```

W+D는 다음 값이 된다.

```text
(1/√2, 1/√2)
≈ (0.707, 0.707)
```

새 길이는 정확히 1이므로 모든 방향에서 최대 이동 속도가 같아진다. 키 입력을 각각 곧바로 `AddForward`와 `AddRight`에 적용하지 않고 먼저 합치는 이유가 이것이다.

---

### 16. 마우스 좌표계: Client와 Screen

마우스 이동량을 얻기 위해 현재는 커서를 게임 클라이언트 영역 중앙으로 되돌리는 임시 방식을 사용한다. 먼저 실제 게임 화면 영역의 크기를 얻는다.

```cpp
RECT clientRect;
GetClientRect(hWnd, &clientRect);
```

Client 영역은 제목 표시줄과 테두리를 제외하고 Direct3D 화면이 그려지는 부분이다. 좌상단이 `(0, 0)`이며 `right`, `bottom`으로 크기를 구할 수 있다.

```cpp
POINT center;
center.x = (clientRect.left + clientRect.right) / 2;
center.y = (clientRect.top + clientRect.bottom) / 2;
```

이 중앙점은 아직 창 내부 기준 Client 좌표다. 그러나 `GetCursorPos`와 `SetCursorPos`는 데스크톱 전체 기준 Screen 좌표를 사용한다. 서로 다른 좌표를 빼면 틀린 이동량이 나오므로 변환한다.

```cpp
ClientToScreen(hWnd, &center);
```

운영체제는 `hWnd`로 창의 화면 위치, 제목 표시줄과 테두리 등을 알고 있으므로 Client 좌표를 Screen 좌표로 바꿀 수 있다.

```text
GetClientRect
→ 게임 화면 내부의 중앙 계산

ClientToScreen
→ 그 중앙을 데스크톱 전체 좌표로 변환

GetCursorPos
→ 현재 커서의 데스크톱 전체 좌표 획득

CursorPosition - Center
→ 이번 마우스 상대 이동량 계산
```

---

### 17. 픽셀 이동량이 라디안 회전량이 되는 과정

`DeltaX`, `DeltaY`의 단위는 픽셀이다. `MouseSpeed`는 이름상 속도지만 실제 의미는 픽셀당 회전 감도다.

```cpp
float MouseSpeed = XMConvertToRadians(0.1f);
```

단위는 다음과 같다.

```text
DeltaX:    pixel
MouseSpeed: radian / pixel

pixel × radian/pixel = radian
```

예를 들어 마우스가 10픽셀 움직이고 감도가 픽셀당 0.1도라면 총 1도 회전한다. 코드에서는 0.1도를 먼저 라디안으로 바꾸었으므로 `AddRotation`에는 라디안 변화량이 전달된다.

```cpp
MainCamera.AddRotation(
    DeltaX * MouseSpeed,
    -DeltaY * MouseSpeed);
```

Windows 화면 좌표는 아래로 갈수록 Y가 커진다. 현재 Camera는 Pitch가 증가할수록 위를 보므로 `DeltaY`에 음수를 붙여 좌표 방향을 뒤집는다.

마우스 이동량에는 `deltaTime`을 다시 곱하지 않는다. `DeltaX`, `DeltaY`가 이미 지난 프레임 사이에 발생한 이동량이기 때문이다. 키보드는 눌림 상태이므로 시간에 따라 이동량을 만들어야 하지만 마우스 델타는 이미 구간 이동량이다.

---

### 18. Pitch를 ±89도로 제한하는 이유

마우스를 계속 위로 움직이면 Pitch가 90도를 넘어 카메라가 뒤집힐 수 있다. 또한 정확히 위나 아래를 바라보면 Forward와 WorldUp이 나란해져 외적 결과의 길이가 0에 가까워진다.

```text
Forward ∥ WorldUp
→ WorldUp × Forward = Zero Vector
→ Right를 정상적으로 정규화할 수 없음
```

따라서 90도에 도달하기 직전인 ±89도로 제한한다.

```cpp
Pitch = std::clamp(
    Pitch,
    XMConvertToRadians(-89.0f),
    XMConvertToRadians(89.0f));
```

`std::clamp(value, low, high)`는 값이 범위보다 작으면 `low`, 크면 `high`, 범위 안이면 원래 값을 반환한다. `std::clamp`는 C++17부터 지원되므로 프로젝트의 C++ 언어 표준을 `/std:c++17`로 설정했다.

---

### 19. 현재 커서 중앙 복귀 방식의 한계

현재 구현은 매 프레임 다음 함수를 호출한다.

```cpp
SetCursorPos(center.x, center.y);
```

이 방식은 간단하지만 운영체제 커서를 실제로 옮기므로 다음 한계가 있다.

```text
커서가 보이면 중앙에서 떨리는 것처럼 보일 수 있음
게임 창이 비활성화되어도 Update가 돌면 커서를 다시 끌어올 수 있음
첫 프레임의 커서 위치가 중앙에서 멀면 카메라가 갑자기 크게 회전할 수 있음
Windows 커서 처리와 가속 설정에 영향을 받을 수 있음
```

현재는 `ShowCursor(FALSE)`로 운영체제 커서를 숨긴다. 단, 화면 중앙에 보이는 FPS 조준점은 운영체제 커서가 아니다. 나중에 Direct3D UI로 별도 렌더링해야 한다.

최종 InputSystem에서는 `WM_INPUT` Raw Input을 사용한다.

```text
현재 임시 방식
GetCursorPos → 중앙과 차이 계산 → SetCursorPos

최종 방식
WM_INPUT → 상대 DeltaX/DeltaY 직접 획득
```

Raw Input으로 바꾸기 전에는 `GetForegroundWindow() == hWnd`로 활성 창인지 확인하고, 처음 활성화된 프레임에는 회전 없이 커서만 중앙으로 옮기는 `MouseInitialized` 상태가 필요하다. 이 처리는 InputSystem 단계에서 추가한다.

---

### 20. CameraBuffer를 매 프레임 갱신하는 흐름

카메라의 Position, Yaw, Pitch는 Update에서 변하지만 GPU는 C++ 객체를 직접 읽을 수 없다. Render에서 계산된 View와 Projection을 Constant Buffer로 복사해야 한다.

```text
Update
→ Position/Yaw/Pitch 변경

Render
→ GetViewMatrix
→ GetProjectionMatrix
→ CameraBuffer Map
→ 행렬 전치 후 저장
→ CameraBuffer Unmap
→ VSSetConstantBuffers
→ DrawIndexed
```

현재 CameraBuffer는 CPU가 자주 쓰는 동적 버퍼다.

```cpp
CameraDesc.Usage = D3D11_USAGE_DYNAMIC;
CameraDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
```

`Map(..., D3D11_MAP_WRITE_DISCARD, ...)`는 이전 내용을 보존할 필요가 없으니 CPU가 새 내용을 쓸 공간을 요청한다. `MappedResource.pData`를 `CameraBufferData*`로 해석해 View와 Projection을 저장하고 `Unmap`하여 기록을 끝낸다.

```cpp
XMStoreFloat4x4(
    &CameraData->View,
    XMMatrixTranspose(MainCamera.GetViewMatrix()));
```

현재 HLSL이 `mul(vector, matrix)` 순서로 행렬을 사용하므로 CPU에서 만든 DirectXMath 행렬을 전치해서 전달한다. 이것은 카메라 수학 자체가 바뀌는 것이 아니라 CPU와 HLSL이 행렬 메모리를 해석하는 규칙을 맞추는 과정이다.

---

### 21. 이번 단계에서 사용한 함수 빠른 사전

| 함수 | 입력과 결과 | 현재 사용 목적 |
|---|---|---|
| `std::sin`, `std::cos` | 라디안 각도 → 삼각함수 값 | Yaw/Pitch로 Forward 계산 |
| `std::sqrt` | 값의 제곱근 | WASD 입력 벡터 길이 계산 |
| `std::clamp` | 값을 최소·최대 범위로 제한 | Pitch를 ±89도로 제한 |
| `XMConvertToRadians` | degree → radian | FOV, Pitch 제한, 마우스 감도 |
| `XMVectorSet` | x, y, z, w로 계산 벡터 생성 | Forward와 WorldUp 생성 |
| `XMLoadFloat3` | `XMFLOAT3` → `XMVECTOR` | Position 계산 시작 |
| `XMStoreFloat3` | `XMVECTOR` → `XMFLOAT3` | 이동 결과 Position 저장 |
| `XMVectorSetY` | Y 성분을 바꾼 새 벡터 반환 | 지면 Forward의 수직 성분 제거 |
| `XMVector3Normalize` | XYZ 방향 유지, 길이 1 | Forward/Right/Up과 이동 방향 정규화 |
| `XMVector3Cross` | 두 벡터의 외적 | Right와 CameraUp 계산 |
| `XMVectorScale` | 벡터 × 스칼라 | 방향에 실제 이동 거리 적용 |
| `XMVectorAdd` | 두 벡터 덧셈 | 현재 위치에 이동량 추가 |
| `XMMatrixLookToLH` | 위치·방향·Up → View 행렬 | 움직이는 FPS 카메라 View 계산 |
| `XMMatrixPerspectiveFovLH` | FOV·종횡비·Near/Far → Projection | 원근 투영 계산 |
| `XMMatrixTranspose` | 행과 열 교환 | HLSL 행렬 해석 규칙에 맞춰 전송 |
| `GetAsyncKeyState` | 키의 현재 비트 상태 | WASD 눌림 확인 |
| `GetClientRect` | HWND → 클라이언트 RECT | 게임 화면 내부 중앙 계산 |
| `ClientToScreen` | 클라이언트 POINT → 화면 POINT | 커서와 같은 좌표계로 변환 |
| `GetCursorPos` | 현재 화면 좌표의 커서 위치 | 중앙과 비교할 마우스 위치 획득 |
| `SetCursorPos` | 화면 좌표로 커서 이동 | 상대 이동을 계속 얻도록 중앙 복귀 |
| `ShowCursor` | 운영체제 커서 표시 카운터 변경 | FPS 조작 중 시스템 커서 숨김 |
| `Map` | GPU 리소스의 CPU 쓰기 주소 요청 | CameraBuffer/ObjectBuffer 갱신 |
| `Unmap` | CPU 접근 종료 | 작성한 버퍼 데이터를 GPU 사용 가능 상태로 전환 |

---

### 22. 현재 한 프레임의 전체 흐름

```text
GameLoop::Run
│
├─ Windows::PeekMSG
├─ Timer::GetDeltaTime
│
├─ LightSaverGame::Update
│  ├─ 큐브 Rotation 증가
│  ├─ WASD 키 상태 수집
│  ├─ 대각선 입력 길이 보정
│  ├─ CameraSpeed × deltaTime 계산
│  ├─ Camera Position 이동
│  ├─ 클라이언트 중앙을 Screen 좌표로 변환
│  ├─ 마우스 DeltaX/DeltaY 계산
│  └─ Camera Yaw/Pitch 갱신
│
├─ LightSaverGame::Render
│  ├─ Camera Forward/Right/Up 계산
│  ├─ View/Projection 계산
│  ├─ CameraBuffer 갱신
│  ├─ 큐브 World 계산
│  ├─ ObjectBuffer 갱신
│  ├─ 렌더링 파이프라인 바인딩
│  └─ DrawIndexed(36, 0, 0)
│
└─ Graphics::Present
```

카메라는 CPU에서 위치와 방향을 계산하고, CameraBuffer는 그 결과를 GPU Vertex Shader에 전달한다. 이 경계를 이해하면 나중에 CameraComponent와 Renderer를 분리할 때 어떤 코드가 어디로 가야 하는지 판단할 수 있다.

---

### 23. 다음 구조 단계와의 연결

현재 `LightSaverGame`은 여전히 큐브의 Mesh, Shader, Constant Buffer와 Draw 명령을 모두 가지고 있다. 다음 단계에서는 결과를 바꾸지 않은 채 책임만 분리한다.

```text
VS + PS + InputLayout + HLSL 컴파일
→ Shader

VertexBuffer + IndexBuffer + Stride + IndexCount
→ Mesh

Shader + Texture + 렌더 상태
→ Material

Transform + Mesh 참조 + Material 참조
→ MeshComponent

CameraBuffer/ObjectBuffer 갱신 + 바인딩 + Draw
→ Renderer
```

Camera는 계속 Position/Yaw/Pitch와 View/Projection 계산을 담당한다. 나중에 Actor/Component 구조가 생기면 Camera 자체를 없애기보다 CameraComponent 또는 PlayerCamera 역할로 확장한다.

---

## 2026-08-17 — Shader와 Mesh 분리, 렌더링 리소스 구조

이번 단계의 목표는 새로운 그래픽 효과를 추가하는 것이 아니라, `LightSaverGame` 안에 한 덩어리로 들어 있던 렌더링 리소스를 역할에 따라 분리하는 것이었다.

분리 전에는 `LightSaverGame`이 다음 일을 전부 수행했다.

```text
LightSaverGame
├─ HLSL 컴파일
├─ Vertex Shader / Pixel Shader 생성
├─ InputLayout 생성
├─ VertexBuffer / IndexBuffer 생성
├─ 모든 리소스 바인딩
├─ 카메라와 오브젝트 상수 버퍼 갱신
└─ DrawIndexed 호출
```

작은 큐브 하나만 그릴 때는 동작하지만 모델과 머티리얼이 늘어나면 어떤 코드가 어떤 리소스를 소유하는지 알기 어려워진다. 그래서 이번에는 화면에 나타나는 결과는 그대로 유지하고 다음 두 책임만 먼저 떼어 냈다.

```text
Shader
├─ HLSL 파일 컴파일
├─ Vertex Shader 소유
├─ Pixel Shader 소유
├─ InputLayout 소유
└─ 파이프라인에 Shader 상태 바인딩

Mesh
├─ VertexBuffer 소유
├─ IndexBuffer 소유
├─ Stride 보관
├─ IndexCount 보관
└─ Input Assembler에 Mesh 상태 바인딩
```

`LightSaverGame`에는 현재 카메라 및 오브젝트 상수 버퍼 갱신과 `DrawIndexed`가 남아 있다. 이는 아직 Renderer를 만들지 않았기 때문이다. 한 번에 모든 구조를 바꾸기보다 실제로 동작하는 상태를 유지하면서 책임을 한 단계씩 이동시키는 과정이다.

---

### 1. Device는 만들고, DeviceContext는 사용한다

Shader와 Mesh 코드에 공통적으로 등장하는 두 포인터는 역할이 다르다.

```cpp
ID3D11Device* Device;
ID3D11DeviceContext* DeviceContext;
```

`ID3D11Device`는 GPU 리소스를 생성할 때 사용한다.

```cpp
Device->CreateVertexShader(...);
Device->CreatePixelShader(...);
Device->CreateInputLayout(...);
Device->CreateBuffer(...);
```

반면 `ID3D11DeviceContext`는 이미 생성한 리소스를 파이프라인에 연결하거나 명령을 기록할 때 사용한다.

```cpp
DeviceContext->IASetInputLayout(...);
DeviceContext->IASetVertexBuffers(...);
DeviceContext->VSSetShader(...);
DeviceContext->PSSetShader(...);
DeviceContext->DrawIndexed(...);
```

쉽게 비유하면 Device는 장비 제작소이고 DeviceContext는 제작된 장비를 현재 작업대에 설치하고 사용하는 조작판이다.

```text
초기화 시점
CPU 데이터 + Device
→ GPU 리소스 생성

렌더링 시점
GPU 리소스 + DeviceContext
→ 파이프라인에 바인딩
→ Draw 명령
```

이 구분 때문에 `Shader::Initialize`와 `Mesh::Initialize`는 Device를 받고, `Shader::Bind`와 `Mesh::Bind`는 DeviceContext를 받는다.

---

### 2. Shader 클래스가 담당하는 범위

현재 Shader 클래스가 소유하는 COM 객체는 세 개다.

```cpp
ID3D11VertexShader* VS = nullptr;
ID3D11PixelShader* PS = nullptr;
ID3D11InputLayout* InputLayout = nullptr;
```

#### 2.1 HLSL 컴파일

```cpp
ID3DBlob* VSBlob = nullptr;
ID3DBlob* PSBlob = nullptr;
ID3DBlob* ErrBlob = nullptr;

HRESULT result = D3DCompileFromFile(
    FilePath,
    nullptr,
    D3D_COMPILE_STANDARD_FILE_INCLUDE,
    "VS",
    "vs_5_0",
    0,
    0,
    &VSBlob,
    &ErrBlob);
```

HLSL 소스 코드는 GPU가 바로 실행할 수 없다. `D3DCompileFromFile`이 텍스트 HLSL을 셰이더 바이트코드로 컴파일하고, 결과를 `ID3DBlob` 안에 보관한다.

- `FilePath`: 컴파일할 HLSL 파일 경로
- `"VS"`: HLSL 안에서 시작할 진입 함수 이름
- `"vs_5_0"`: Vertex Shader 5.0 프로필
- `&VSBlob`: 성공했을 때 컴파일된 바이트코드를 받을 곳
- `&ErrBlob`: 실패했을 때 컴파일 메시지를 받을 곳
- 반환값 `HRESULT`: 함수 성공 또는 실패 여부

Pixel Shader도 같은 방식이며 진입점과 프로필만 `PS`, `ps_5_0`으로 달라진다.

#### 2.2 Blob은 Shader 자체가 아니다

`VSBlob`은 Vertex Shader 객체가 아니라 컴파일된 바이트코드를 담는 임시 메모리다.

```text
Shader.hlsl 텍스트
    ↓ D3DCompileFromFile
VSBlob 바이트코드
    ↓ CreateVertexShader
ID3D11VertexShader GPU 객체
```

또한 InputLayout 생성에도 Vertex Shader의 입력 서명이 필요하므로 같은 `VSBlob`을 사용한다.

```cpp
Device->CreateInputLayout(
    layout,
    ARRAYSIZE(layout),
    VSBlob->GetBufferPointer(),
    VSBlob->GetBufferSize(),
    &InputLayout);
```

Vertex Shader와 InputLayout 생성이 끝나면 Blob의 임무도 끝난다. 따라서 성공 경로 마지막에서 `Release`한다.

```cpp
VSBlob->Release();
PSBlob->Release();
```

현재 코드는 컴파일이나 생성 도중 실패하는 경로에서 이미 만들어진 Blob이 남을 수 있다. 기능을 먼저 분리한 현재 단계의 정리 대상이며, 이후에는 `Microsoft::WRL::ComPtr`를 사용하거나 모든 실패 경로를 한곳에서 정리하도록 개선한다.

#### 2.3 컴파일 오류 문자열 확인

HLSL 컴파일에 실패하면 `ErrBlob`에는 사람이 읽을 수 있는 오류 메시지가 들어갈 수 있다.

```cpp
if (ErrBlob != nullptr)
{
    OutputDebugStringA(
        static_cast<const char*>(
            ErrBlob->GetBufferPointer()));
}
```

`GetBufferPointer()`의 반환형은 `void*`다. Blob은 어떤 종류의 바이트든 담을 수 있으므로 Direct3D가 그 안의 실제 자료형을 강제하지 않는다. 여기서는 내용이 ANSI 문자열이라는 것을 알고 있으므로 `const char*`로 해석한다.

`OutputDebugStringA`의 `A`는 `char` 문자열 버전을 의미한다. 이 캐스팅은 문자열을 변환하는 연산이 아니라, 같은 메모리를 문자 배열로 해석하겠다고 컴파일러에 알려 주는 것이다. 출력은 Visual Studio의 Output 창에서 확인한다.

`ErrBlob` 역시 COM 객체이므로 사용이 끝나면 `Release`해야 한다. 이 오류 출력과 실패 경로 정리는 다음 리팩터링 때 추가할 항목이다.

#### 2.4 Shader 객체 생성과 바인딩은 다르다

```cpp
Device->CreateVertexShader(..., &VS);
Device->CreatePixelShader(..., &PS);
```

위 코드는 GPU에서 사용할 객체를 생성할 뿐, 현재 Draw에 사용할 셰이더로 선택한 것은 아니다. 실제 파이프라인 연결은 `Bind`가 수행한다.

```cpp
void Shader::Bind(ID3D11DeviceContext* DeviceContext)
{
    DeviceContext->IASetInputLayout(InputLayout);
    DeviceContext->VSSetShader(VS, nullptr, 0);
    DeviceContext->PSSetShader(PS, nullptr, 0);
}
```

여러 셰이더를 사용하는 게임에서는 오브젝트나 패스마다 현재 셰이더가 바뀔 수 있으므로 생성과 바인딩을 구분해야 한다.

---

### 3. InputLayout은 Vertex Buffer의 설명서다

현재 정점 구조는 위치만 갖는다.

```cpp
struct Vertex
{
    float x;
    float y;
    float z;
};
```

이에 대응하는 InputLayout은 다음과 같다.

```cpp
D3D11_INPUT_ELEMENT_DESC layout[] =
{
    {
        "POSITION",
        0,
        DXGI_FORMAT_R32G32B32_FLOAT,
        0,
        0,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
};
```

GPU는 VertexBuffer를 그저 연속된 바이트로 본다. InputLayout이 있어야 한 정점에서 몇 바이트를 어떤 형식과 Semantic으로 읽을지 알 수 있다.

```text
VertexBuffer의 12바이트
┌───────────┬───────────┬───────────┐
│ float x   │ float y   │ float z   │
└───────────┴───────────┴───────────┘
              ↓ InputLayout
HLSL의 float3 Position : POSITION
```

다음 텍스처 단계에서 정점에 UV가 추가되면 구조와 InputLayout을 함께 바꿔야 한다.

```cpp
struct Vertex
{
    DirectX::XMFLOAT3 Position; // 12 bytes
    DirectX::XMFLOAT2 UV;       // 8 bytes
};                              // total 20 bytes
```

```cpp
{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
```

InputLayout의 Semantic과 HLSL 입력 구조의 Semantic은 일치해야 한다.

---

### 4. Mesh는 모양을 그리는 GPU 데이터 묶음이다

현재 Mesh 클래스는 다음 상태를 소유한다.

```cpp
ID3D11Buffer* VertexBuffer = nullptr;
ID3D11Buffer* IndexBuffer = nullptr;
UINT Stride = sizeof(Vertex);
UINT IndexCount = 0;
```

- `VertexBuffer`: 각 정점의 위치 데이터
- `IndexBuffer`: 어떤 정점을 어떤 순서로 재사용할지 나타내는 번호
- `Stride`: 다음 정점까지 이동해야 할 바이트 수
- `IndexCount`: `DrawIndexed`가 읽을 인덱스 개수

Mesh는 월드 위치, 카메라, HLSL 코드 또는 텍스처를 직접 소유하지 않는다. 현재 단계에서 Mesh의 핵심은 정점과 인덱스라는 기하 데이터다.

#### 4.1 CPU 배열에서 GPU Buffer 만들기

```cpp
D3D11_BUFFER_DESC VertexBufferDesc = {};
VertexBufferDesc.ByteWidth = sizeof(Vertex) * vertexCount;
VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

D3D11_SUBRESOURCE_DATA VertexData = {};
VertexData.pSysMem = vertices;

Device->CreateBuffer(
    &VertexBufferDesc,
    &VertexData,
    &VertexBuffer);
```

`ByteWidth`는 요소 개수가 아니라 전체 바이트 수다. 정점 하나의 크기와 정점 개수를 곱해야 한다.

```text
정점 1개의 크기 = sizeof(Vertex) = 12 bytes
정점이 8개라면 = 12 × 8 = 96 bytes
```

IndexBuffer도 같은 원리다.

```cpp
IndexBufferDesc.ByteWidth = sizeof(UINT) * indexCount;
IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
IndexData.pSysMem = indices;
```

현재 큐브의 기하 데이터는 실행 중 바뀌지 않으므로 `D3D11_USAGE_IMMUTABLE`이 맞다. 카메라나 World 행렬처럼 매 프레임 바뀌는 값은 Dynamic Constant Buffer와 `Map/Unmap`을 사용하지만, 움직이지 않는 정점 데이터까지 매 프레임 다시 쓸 필요는 없다.

#### 4.2 Stride는 정점 사이의 간격이다

```cpp
UINT Stride = sizeof(Vertex);
```

GPU가 첫 번째 정점을 읽은 다음 두 번째 정점으로 가려면 메모리에서 몇 바이트 이동할지 알아야 한다. 이 간격이 Stride다.

```text
주소 0          : Vertex 0
주소 Stride     : Vertex 1
주소 Stride × 2 : Vertex 2
```

정점에 UV를 추가하여 `sizeof(Vertex)`가 20바이트가 되면 Stride도 자동으로 20바이트가 된다.

---

### 5. IASetVertexBuffers에서 발생했던 접근 위반

Mesh 분리 후 다음 호출에서 `0xC0000005` 읽기 접근 위반이 발생했다.

```cpp
DeviceContext->IASetVertexBuffers(
    0,
    1,
    &VertexBuffer,
    &Stride,
    0);
```

마지막 인자는 숫자 하나가 아니라 `const UINT* pOffsets`, 즉 오프셋 배열의 주소다.

```cpp
void IASetVertexBuffers(
    UINT StartSlot,
    UINT NumBuffers,
    ID3D11Buffer* const* ppVertexBuffers,
    const UINT* pStrides,
    const UINT* pOffsets);
```

`NumBuffers`가 1이면 Direct3D는 Stride 한 개와 Offset 한 개를 읽으려고 한다. 그런데 마지막에 `0`을 넘기면 주소 0, 즉 null pointer에서 값을 읽으려 하므로 접근 위반이 발생할 수 있다.

올바른 코드는 실제 `UINT` 변수를 만들고 그 주소를 넘기는 것이다.

```cpp
UINT Offset = 0;

DeviceContext->IASetVertexBuffers(
    0,
    1,
    &VertexBuffer,
    &Stride,
    &Offset);
```

반면 `IASetIndexBuffer`의 세 번째 인자는 포인터가 아닌 `UINT Offset` 값 자체다.

```cpp
DeviceContext->IASetIndexBuffer(
    IndexBuffer,
    DXGI_FORMAT_R32_UINT,
    Offset);
```

두 함수가 비슷해 보이지만 인자의 형식이 다르다.

```text
IASetVertexBuffers(..., &Stride, &Offset)
                              주소     주소

IASetIndexBuffer(..., DXGI_FORMAT_R32_UINT, Offset)
                                            값
```

VertexBuffer는 한 번에 여러 슬롯을 묶어서 지정할 수 있으므로 버퍼별 Stride와 Offset을 배열로 받는다. IndexBuffer는 한 번에 하나만 지정하므로 Offset 하나를 값으로 받는다.

---

### 6. Bind는 초기화가 아니라 Draw 직전의 상태 선택이다

VertexBuffer와 IndexBuffer를 한 번 만들었다고 해서 영원히 현재 파이프라인에 연결되어 있는 것은 아니다. Direct3D 11의 DeviceContext는 현재 상태를 갖는 상태 머신이다.

```text
Mesh A Bind
→ 현재 IA에는 Mesh A가 연결됨

Mesh B Bind
→ 같은 슬롯이 Mesh B로 교체됨

Mesh A를 다시 그리려면
→ Mesh A를 다시 Bind해야 함
```

그래서 Mesh의 GPU 버퍼 생성은 초기화 때 한 번 수행하지만, `Mesh::Bind`는 그 Mesh를 그리기 직전에 호출한다.

현재 한 프레임의 핵심 렌더 순서는 다음과 같다.

```cpp
DeviceContext->IASetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

ShaderSet.Bind(DeviceContext);
MeshSet.Bind(DeviceContext);

DeviceContext->DrawIndexed(
    MeshSet.GetIndexCount(),
    0,
    0);
```

이를 파이프라인 흐름으로 보면 다음과 같다.

```text
Mesh::Bind
→ IA가 Vertex/Index 데이터를 읽을 준비

Shader::Bind
→ IA의 데이터 해석법과 VS/PS 선택

Constant Buffer Bind
→ World/View/Projection 전달

DrawIndexed
→ 현재까지 설정된 모든 상태를 사용해 실제 그리기 명령 실행
```

`DrawIndexed`를 Mesh 안에 넣지 않은 이유는 Mesh가 스스로 언제, 어떤 Shader와 Material, 어떤 Transform으로 그려질지 결정하게 만들지 않기 위해서다. 이후 Renderer가 이 조합과 Draw 호출을 담당하게 된다.

---

### 7. 값 멤버와 포인터 멤버의 차이

초기 분리 과정에서 Shader를 다음과 같이 선언하면 문제가 생길 수 있었다.

```cpp
Shader* ShaderSet = nullptr;
ShaderSet->Initialize(...); // nullptr을 역참조
```

포인터 변수만 만든 것은 Shader 객체를 생성한 것이 아니다. `ShaderSet`은 아무 객체도 가리키지 않으므로 멤버 함수를 호출할 수 없다.

현재처럼 게임이 Shader 하나와 Mesh 하나를 직접 소유하는 단계에서는 값 멤버가 간단하다.

```cpp
Shader ShaderSet;
Mesh MeshSet;

ShaderSet.Initialize(...);
MeshSet.Initialize(...);
```

`LightSaverGame`이 만들어질 때 두 객체도 함께 만들어지고, `LightSaverGame`이 파괴될 때 각 객체의 소멸자도 자동 호출된다. 따라서 현재 단계에서는 별도의 `new/delete`가 필요 없다.

나중에 `ResourceManager`가 여러 Mesh와 Shader를 공유하게 되면 `shared_ptr`, `unique_ptr`, 핸들 또는 리소스 ID 같은 소유 방식을 다시 설계한다. 지금부터 모든 것을 포인터로 만들 필요는 없다.

---

### 8. COM 객체와 소멸자

Direct3D 인터페이스는 COM 객체이므로 사용이 끝나면 `Release`해야 한다.

```cpp
Shader::~Shader()
{
    if (VS != nullptr)
    {
        VS->Release();
        VS = nullptr;
    }

    if (PS != nullptr)
    {
        PS->Release();
        PS = nullptr;
    }

    if (InputLayout != nullptr)
    {
        InputLayout->Release();
        InputLayout = nullptr;
    }
}
```

Mesh의 VertexBuffer와 IndexBuffer도 같은 방식으로 정리한다. `nullptr` 검사는 객체 생성이 일부만 성공했거나 초기화가 중간에 실패한 경우에도 소멸자가 안전하게 실행되도록 한다.

현재 방식은 COM 수명 관리를 직접 배우는 단계에는 도움이 된다. 반복되는 `Release`와 실패 경로 누락을 줄이는 다음 수단은 `Microsoft::WRL::ComPtr<T>`다.

```cpp
Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
```

ComPtr을 사용하면 객체의 수명이 끝날 때 자동으로 Release된다. 다만 자동화 도구를 쓰기 전 현재의 `AddRef/Release` 원리를 먼저 이해하는 것이 중요하다.

---

### 9. OpenGL AimLab 프로젝트와 현재 구조 비교

참고한 OpenGL AimLab 프로젝트에서는 저수준 Mesh 구현을 게임 코드에서 직접 작성하지 않았다.

```cpp
using CpuMesh = glutil::ModelData;
using Mesh = glutil::GLModelData;
```

별도의 `glutil` 라이브러리가 OBJ 로딩과 VAO/VBO/EBO 생성 과정을 감췄다. 그래서 게임 코드에서는 경로를 넘기고 이미 만들어진 Mesh를 받아 사용하는 것처럼 보였다.

현재 LightSaver에서 직접 만든 것과 대응시키면 다음과 같다.

| OpenGL / AimLab | Direct3D 11 / LightSaver |
|---|---|
| VBO | VertexBuffer |
| EBO | IndexBuffer |
| VAO에 저장된 정점 해석 상태 | VertexBuffer 바인딩 + InputLayout |
| `glDrawElements` | `DrawIndexed` |
| `GLProgram` | Shader |
| model uniform | Object Constant Buffer |
| `GLModelData` | 현재 Mesh |
| Model loader가 만든 CPU 데이터 | 미래의 MeshData |

즉, AimLab에서 경로만 넣으면 되었던 것은 Mesh가 필요 없어서가 아니라 로더와 유틸리티 라이브러리 안에 현재 우리가 배우는 과정이 이미 들어 있었기 때문이다.

AimLab의 큰 흐름은 다음과 같았다.

```text
ModelLoader
→ CPU ModelData
→ GLModelData(VAO/VBO/EBO)
→ ResourceManager가 캐시
→ MeshRenderer가 Mesh와 Material 참조
→ Render 시 DrawElements
```

LightSaver의 목표 흐름도 같은 개념을 Direct3D 11에 맞춰 다음처럼 발전시킨다.

```text
ModelLoader
→ CPU MeshData(Vertex/Index 배열)
→ Mesh가 GPU Vertex/Index Buffer 생성
→ ResourceManager가 Mesh/Texture/Shader 캐시
→ MeshComponent가 Mesh/Material 참조
→ Renderer가 Bind와 DrawIndexed 실행
```

다만 AimLab의 전역 싱글톤, raw pointer 중심 소유, 컴포넌트가 직접 Draw하는 방식까지 그대로 복사하지는 않는다. 참고할 것은 리소스 로딩과 렌더링 데이터의 분리 원리다.

---

### 10. Texture, Material, ModelLoader는 각각 무엇인가

현재 단계 이후의 클래스는 서로 같은 일을 하지 않는다.

#### Texture

이미지 파일의 픽셀을 GPU 리소스로 올리고 Pixel Shader가 읽을 수 있는 `ShaderResourceView`를 제공한다.

```text
PNG/DDS 파일
→ 픽셀 디코딩
→ ID3D11Texture2D
→ ID3D11ShaderResourceView
→ Pixel Shader에서 Sample
```

#### Material

어떤 Shader와 Texture, 그리고 색상·거칠기 같은 파라미터를 함께 사용할지 나타낸다.

```text
Material
├─ Shader 참조
├─ Texture 참조
├─ SamplerState 참조
└─ 색상/광택 등의 파라미터
```

#### ModelLoader

OBJ 또는 glTF 같은 모델 파일을 읽어 CPU에서 사용할 정점과 인덱스 데이터로 바꾼다. GPU 버퍼를 직접 소유하는 Mesh와 파일 형식을 해석하는 ModelLoader의 책임을 구분한다.

```text
모델 파일
→ ModelLoader
→ MeshData
→ Mesh::Initialize
→ GPU Buffer
```

#### ResourceManager

같은 파일을 여러 번 로드하지 않도록 경로 또는 이름으로 리소스를 캐시하고 공유한다.

```text
"wall.png" 요청
→ 이미 캐시에 있으면 기존 Texture 반환
→ 없으면 로드한 뒤 캐시에 저장하고 반환
```

로더와 ResourceManager는 최종 목표가 아니다. 실제 공포 게임의 맵, 손전등, 괴물, 발전기를 데이터로 출력하기 위한 기반이다.

---

### 11. 셰이더 학습이 실제 공포 게임으로 이어지는 지점

현재 Shader 클래스는 단순 색상 출력만 다루지만 프로젝트의 핵심 기믹은 셰이더 학습과 직접 연결된다.

```text
Texture sampling
→ 모델 표면에 이미지 표현

Normal + 기본 조명
→ 벽과 괴물의 입체감

Spot Light
→ 손전등의 원뿔형 빛

Shadow Map + PCF
→ 손전등 뒤의 그림자와 공포 연출

Fog
→ 거리 가시성 제한

Post Process
→ 비네트, 노이즈, 피격/공포 효과
```

몬스터가 손전등 범위 안에서 멈추는 판정은 CPU 게임플레이 로직이 담당한다. 손전등이 실제로 표면을 밝히는 것은 GPU Shader가 담당한다. 두 기능은 같은 위치, 방향, 각도 개념을 공유하지만 목적이 다르다.

```text
CPU
플레이어 → 몬스터 방향
거리, 내적, 가림 여부 판정
→ 몬스터 이동 정지

GPU
각 픽셀 → 손전등 방향/거리 계산
조명과 그림자 계산
→ 화면에 빛 표현
```

이 둘을 같은 데이터에서 출발하게 만들면 화면에서는 빛이 닿는데 몬스터는 움직이거나, 빛이 닿지 않는데 멈추는 불일치를 줄일 수 있다.

---

### 12. 소리는 그래픽 Shader가 아니다

소리도 파동이지만 HLSL 그래픽 셰이더로 처리하는 것은 아니다. 그래픽 셰이더는 GPU에서 정점과 픽셀을 계산하고, 소리는 Audio Engine 또는 DSP가 시간에 따른 샘플을 계산한다.

```text
그래픽
공간의 Vertex/Pixel
→ GPU Shader
→ 화면

오디오
시간에 따른 Sample
→ Audio Engine / DSP
→ 스피커와 헤드폰
```

공포 게임에서 필요한 3D 소리에는 다음 개념이 연결된다.

- 거리 감쇠: 멀어질수록 볼륨 감소
- 좌우 방향: 카메라 Right와 소리 방향의 내적으로 패닝 결정
- 앞뒤/위아래 구분: HRTF 같은 공간 음향 처리
- 벽 가림: Raycast 후 볼륨 감소와 Low-pass filter 적용
- 공간감: Reverb와 방 크기 설정

따라서 렌더링 Shader와 오디오를 별도의 시스템으로 두되, 플레이어와 소리 발생원의 Transform은 World에서 함께 공유한다.

---

### 13. 현재 완료 상태와 바로 다음 과제

현재까지 완성된 기반은 다음과 같다.

```text
WinMain
→ GameLoop
   ├─ Window
   ├─ Timer
   ├─ Graphics
   └─ LightSaverGame
      ├─ Camera
      ├─ Shader
      ├─ Mesh
      ├─ ObjectBuffer
      └─ CameraBuffer
```

현재 출력 결과는 WASD와 마우스로 카메라를 움직이며 회전하는 색상 큐브다. 렌더링 데이터 측면에서는 Shader와 Mesh의 첫 분리가 끝났다.

바로 다음 과제는 모델 로더가 아니라 하드코딩된 큐브에 텍스처를 붙이는 것이다.

```text
1. Vertex에 UV 추가
2. InputLayout에 TEXCOORD 추가
3. HLSL VS 입력/출력에 UV 연결
4. Texture2D와 ShaderResourceView 생성
5. SamplerState 생성
6. Pixel Shader에서 Texture.Sample
7. 큐브의 24개 정점과 면별 UV 구성
```

큐브를 위치 정점 8개로 만들면 위치는 공유할 수 있지만 각 면이 서로 다른 UV와 Normal을 갖기 어렵다. 텍스처와 조명을 위한 일반적인 큐브는 면마다 네 정점을 두어 24개 정점으로 구성한다.

그 다음 확장 순서는 다음과 같다.

```text
Texture
→ Material
→ ModelLoader
→ ResourceManager
→ World / Actor / Component
→ Renderer
→ 맵과 충돌
→ 손전등
→ 몬스터 두 마리
→ 발전기와 출구
→ 안개, 3D 오디오, 레이더
```

이번 분리의 핵심은 클래스를 많이 만드는 것이 아니라, 앞으로 실제 게임 요소가 늘어나도 같은 Mesh와 Shader를 재사용하고 조합할 수 있는 첫 경계를 세운 것이다.

---

## 2026-08-18 — UV 좌표, WIC 텍스처 로딩, SRV와 Sampler

### 오늘의 목표와 결과

오늘의 목표는 단색으로 출력하던 큐브에 이미지 텍스처를 입히고, 이미지 파일이 Pixel Shader의 최종 색상이 되기까지의 전체 경로를 직접 구현하는 것이었다.

구현한 항목은 다음과 같다.

1. `Vertex`에 2차원 UV 좌표 추가
2. 큐브를 위치 정점 8개가 아니라 면별 정점 24개로 재구성
3. `AddFace`를 사용하여 각 면의 정점과 인덱스를 생성
4. Input Layout에 `TEXCOORD` 요소 추가
5. Vertex Shader에서 UV를 Pixel Shader로 전달
6. WinMain의 메인 스레드를 COM에 등록
7. WIC로 JPEG 파일을 열고 RGBA 픽셀 배열로 변환
8. CPU 픽셀 배열로 `ID3D11Texture2D` 생성
9. Texture2D를 Shader에서 읽기 위한 SRV 생성
10. 텍스처 주소 지정과 필터링 규칙을 가진 Sampler State 생성
11. SRV를 Pixel Shader의 `t0`, Sampler를 `s0` 슬롯에 연결
12. HLSL의 `Texture2D.Sample`로 큐브 표면의 최종 색상 출력

최종 데이터 흐름은 다음과 같다.

```text
Test.jpg
→ WIC Decoder
→ 첫 번째 Image Frame
→ Format Converter(RGBA 32비트)
→ CPU의 연속된 PixelData 배열
→ ID3D11Texture2D
→ ID3D11ShaderResourceView
→ PSSetShaderResources(t0)
→ Texture2D DiffuseTexture : register(t0)
→ Texture.Sample(Sampler, UV)
→ Pixel Shader의 SV_TARGET
→ Render Target / Back Buffer
→ 화면
```

---

### 1. UV 좌표는 모델 표면과 이미지 위치를 연결한다

위치 좌표는 정점이 3차원 공간의 어디에 있는지 나타내고, UV 좌표는 그 정점에 이미지의 어느 위치를 붙일지 나타낸다.

```cpp
struct Vertex
{
    float x, y, z;
    float u, v;
};
```

현재 정점 하나의 메모리 배치는 다음과 같다.

```text
0 byte                                      20 byte
├──── x ────┼──── y ────┼──── z ────┼──── u ────┼──── v ────┤
    4           4           4           4           4 bytes

Position = 처음 12바이트
UV       = 그 다음 8바이트
Stride   = sizeof(Vertex) = 20바이트
```

일반적으로 UV의 범위는 다음과 같다.

```text
(0, 0) ---------------- (1, 0)
  |                        |
  |       이미지           |
  |                        |
(0, 1) ---------------- (1, 1)
```

UV는 화면 좌표가 아니다. 화면의 중앙이 `(0, 0)`인 NDC 좌표와도 다르다. UV는 모델 표면에서 텍스처 이미지를 조회하기 위한 독립적인 2차원 좌표다.

현재 큐브 한 면에는 다음 UV를 사용한다.

```cpp
vertices.push_back({ v0.x, v0.y, v0.z, 0.0f, 1.0f });
vertices.push_back({ v1.x, v1.y, v1.z, 0.0f, 0.0f });
vertices.push_back({ v2.x, v2.y, v2.z, 1.0f, 0.0f });
vertices.push_back({ v3.x, v3.y, v3.z, 1.0f, 1.0f });
```

이 네 정점은 이미지의 네 모서리를 큐브 한 면의 네 모서리에 대응시킨다.

---

### 2. 텍스처 큐브가 8개가 아니라 24개 정점을 사용하는 이유

위치만 생각하면 큐브에는 서로 다른 모서리가 8개뿐이다. 그러나 렌더링 정점은 위치만으로 같은 정점인지 결정되지 않는다.

```text
렌더링 정점
= Position
+ UV
+ Normal
+ 이후 추가될 Tangent, Color 등의 속성
```

큐브 모서리 하나는 세 면이 공유한다. 같은 3차원 위치라도 각 면에서는 서로 다른 UV와 Normal이 필요하다.

예를 들어 한 모서리의 위치가 같더라도 다음처럼 의미가 달라질 수 있다.

```text
앞면에서의 정점: Position A, UV (0, 0), Normal (0, 0, -1)
왼쪽 면의 정점: Position A, UV (1, 0), Normal (-1, 0, 0)
위쪽 면의 정점: Position A, UV (0, 1), Normal (0, 1, 0)
```

따라서 면마다 정점 네 개를 독립적으로 만들어야 한다.

```text
6 faces × 4 vertices = 24 vertices
6 faces × 2 triangles × 3 indices = 36 indices
```

`AddFace`는 네 위치를 받아 정점 네 개와 삼각형 두 개를 생성한다.

```text
v0 ---- v1
|     /  |
|   /    |
v3 ---- v2

Triangle 1: 0, 1, 2
Triangle 2: 0, 2, 3
```

각 면을 호출할 때 넘기는 위치 순서가 컬링 방향을 결정하고, `AddFace` 내부의 UV 순서가 텍스처의 방향을 결정한다.

---

### 3. Input Layout과 HLSL Semantic 연결

CPU의 `Vertex`에 UV를 추가했으므로 GPU가 새 메모리 배치를 읽는 방법도 알려 줘야 한다.

```cpp
D3D11_INPUT_ELEMENT_DESC layout[] =
{
    {
        "POSITION", 0,
        DXGI_FORMAT_R32G32B32_FLOAT,
        0, 0,
        D3D11_INPUT_PER_VERTEX_DATA, 0
    },
    {
        "TEXCOORD", 0,
        DXGI_FORMAT_R32G32_FLOAT,
        0, 12,
        D3D11_INPUT_PER_VERTEX_DATA, 0
    }
};
```

- POSITION은 `float` 세 개이므로 `R32G32B32_FLOAT`다.
- TEXCOORD는 `float` 두 개이므로 `R32G32_FLOAT`다.
- TEXCOORD의 시작 오프셋은 Position 12바이트 다음인 12다.

HLSL 입력 구조의 Semantic이 같은 이름으로 연결된다.

```hlsl
struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};
```

변수 이름 `position`, `texcoord` 자체보다 `POSITION`, `TEXCOORD` Semantic이 파이프라인 연결에서 중요하다.

---

### 4. UV는 Vertex Shader에서 Pixel Shader로 어떻게 전달되는가

Vertex Shader는 입력받은 UV를 출력 구조체에 넣는다.

```hlsl
output.texcoord = input.texcoord;
```

하지만 Pixel Shader는 정점에서만 실행되는 것이 아니다. Rasterizer가 삼각형 내부를 픽셀 조각으로 만들면서 세 정점의 UV를 보간한다.

```text
삼각형 세 정점의 UV
→ Vertex Shader 출력
→ Rasterizer
→ 삼각형 내부 위치에 맞춰 UV 보간
→ 픽셀마다 서로 다른 PS_INPUT.texcoord 생성
→ Pixel Shader에서 텍스처 조회
```

예를 들어 한쪽 정점의 U가 0이고 반대쪽 정점의 U가 1이라면, 그 사이 픽셀에는 0.1, 0.2, 0.3 같은 중간 U가 자동으로 만들어진다. 그래서 네 모서리의 UV만 지정해도 이미지 전체가 면 위에 연속적으로 펼쳐진다.

---

### 5. COM 초기화와 WIC

WIC는 Windows Imaging Component이며 COM 기반 API다. 따라서 WIC 객체를 생성하기 전에 현재 스레드가 COM을 사용할 것이라고 등록해야 한다.

```cpp
HRESULT result = CoInitializeEx(
    nullptr,
    COINIT_MULTITHREADED);
```

`CoInitializeEx`는 새 작업 스레드를 생성하는 함수가 아니다. 현재 호출한 스레드의 COM 동작 모델을 초기화하는 함수다.

- `S_OK`: 현재 스레드에서 COM이 처음 초기화됨
- `S_FALSE`: 이미 같은 방식으로 초기화되어 있음. 이것도 성공이다.
- `RPC_E_CHANGED_MODE`: 같은 스레드가 이미 다른 COM 동작 모델로 초기화됨

따라서 단순히 `result != S_OK`가 아니라 `FAILED(result)`로 실패를 검사해야 한다.

```cpp
if (FAILED(result))
{
    return 0;
}
```

성공한 `CoInitializeEx` 호출에는 대응하는 `CoUninitialize`가 필요하다.

```text
CoInitializeEx
→ COM/WIC 객체 사용
→ COM 객체 모두 파괴
→ CoUninitialize
```

현재 학습 코드에서는 이 수명 순서를 더 명확하게 만들기 위해 `LightSaverGame`의 생존 범위를 별도 블록으로 제한하는 개선을 이후 적용할 수 있다.

---

### 6. WIC 객체들의 역할

수동 WIC 로더의 핵심 객체는 네 개다.

#### 6.1 IWICImagingFactory

```cpp
CoCreateInstance(
    CLSID_WICImagingFactory,
    nullptr,
    CLSCTX_INPROC_SERVER,
    IID_PPV_ARGS(&ImageFactory));
```

WIC Decoder와 Format Converter 같은 객체를 생성하는 공장이다. 이미지 데이터 자체는 아니다.

#### 6.2 IWICBitmapDecoder

```cpp
ImageFactory->CreateDecoderFromFilename(
    FilePath,
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnDemand,
    &Decoder);
```

JPEG, PNG 같은 파일 컨테이너를 열고 파일 형식을 해석한다. `WICDecodeMetadataCacheOnDemand`는 필요한 시점에 메타데이터를 읽도록 한다.

#### 6.3 IWICBitmapFrameDecode

```cpp
Decoder->GetFrame(0, &Frame);
```

파일 안에서 실제 이미지 프레임 한 장을 선택한다. 일반적인 JPEG나 PNG는 한 프레임이지만 GIF나 다중 프레임 이미지 형식은 여러 프레임을 가질 수 있다.

#### 6.4 IWICFormatConverter

```cpp
Converter->Initialize(
    Frame,
    GUID_WICPixelFormat32bppRGBA,
    WICBitmapDitherTypeNone,
    nullptr,
    0.0f,
    WICBitmapPaletteTypeCustom);
```

입력 이미지가 RGB, BGR, 팔레트 형식 등 무엇이든 GPU로 넘기기 쉬운 32비트 RGBA 형식으로 통일한다.

```text
한 픽셀 = R 1바이트 + G 1바이트 + B 1바이트 + A 1바이트
        = 총 4바이트
```

Format Converter의 설정만으로 CPU 배열이 생기는 것은 아니다. 실제 디코딩·변환 결과를 메모리에 복사하는 호출이 `CopyPixels`다.

---

### 7. CopyPixels와 2차원 이미지의 메모리 배치

이미지는 가로와 세로를 가진 2차원 자료지만, 메모리에서는 연속된 1차원 바이트 배열로 저장한다.

```cpp
const UINT BytesPerPixel = 4;
const UINT Stride = Width * BytesPerPixel;
const UINT Size = Stride * Height;

std::vector<unsigned char> PixelData(Size);
```

여기서 코드의 `Stride`는 한 이미지 행이 차지하는 바이트 수다. Direct3D에서는 같은 의미로 `RowPitch`라는 이름을 많이 사용한다.

```text
RowPitch = Width × 4 bytes
TotalSize = RowPitch × Height
```

가로 3픽셀, 세로 2픽셀이라면 다음과 같다.

```text
2차원 이미지
[P00][P01][P02]
[P10][P11][P12]

메모리
[P00 RGBA][P01 RGBA][P02 RGBA][P10 RGBA][P11 RGBA][P12 RGBA]
```

실제 픽셀 복사는 다음 호출에서 일어난다.

```cpp
Converter->CopyPixels(
    nullptr,
    Stride,
    Size,
    PixelData.data());
```

- 첫 번째 `nullptr`: 이미지 전체 영역 복사
- `Stride`: 목적지 한 행의 바이트 간격
- `Size`: 목적지 배열 전체 크기
- `PixelData.data()`: 실제 바이트를 받을 CPU 메모리 주소

이 단계가 끝나도 데이터는 아직 CPU 메모리에 있다. GPU가 Shader에서 읽으려면 Direct3D Texture Resource로 다시 생성해야 한다.

---

### 8. ID3D11Texture2D 생성

Texture2D의 모양과 용도는 `D3D11_TEXTURE2D_DESC`로 설명한다.

```cpp
D3D11_TEXTURE2D_DESC TextureDesc = {};
TextureDesc.Width = Width;
TextureDesc.Height = Height;
TextureDesc.MipLevels = 1;
TextureDesc.ArraySize = 1;
TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
TextureDesc.SampleDesc.Count = 1;
TextureDesc.SampleDesc.Quality = 0;
TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
```

#### Format

WIC Converter의 `GUID_WICPixelFormat32bppRGBA`와 Direct3D의 `DXGI_FORMAT_R8G8B8A8_UNORM`을 일치시켰다.

`UNORM`은 각 8비트 채널의 정수 범위 0~255를 Shader에서 0.0~1.0 범위로 정규화해서 읽는다는 의미다.

#### Usage

이미지 파일에서 한 번 로드한 뒤 실행 중 내용을 바꾸지 않으므로 `D3D11_USAGE_IMMUTABLE`을 사용한다. Immutable Resource는 생성할 때 반드시 초기 데이터를 함께 제공해야 한다.

#### BindFlags

```cpp
D3D11_BIND_SHADER_RESOURCE
```

이 Texture2D를 Shader에서 읽을 리소스로 사용할 것이라고 지정한다.

#### SampleDesc.Count와 Quality

```cpp
TextureDesc.SampleDesc.Count = 1;
TextureDesc.SampleDesc.Quality = 0;
```

이 `Quality`는 JPEG 화질, 해상도 또는 텍스처 필터 품질이 아니다. `SampleDesc`는 MSAA의 픽셀당 샘플 수와 장치가 지원하는 샘플 배치 패턴을 지정한다.

- `Count = 1`: 픽셀당 샘플 하나, 즉 MSAA를 사용하지 않음
- `Quality = 0`: 기본 샘플 패턴

MSAA를 사용할 때도 Quality 숫자가 클수록 무조건 화질이 좋다는 뜻은 아니다. 지원 가능한 패턴 수는 `CheckMultisampleQualityLevels`로 조회해야 한다. 일반적인 Shader Resource용 이미지 텍스처는 `Count = 1`, `Quality = 0`을 사용한다.

---

### 9. D3D11_SUBRESOURCE_DATA는 CPU 초기 데이터의 설명서다

```cpp
D3D11_SUBRESOURCE_DATA InitialData = {};
InitialData.pSysMem = PixelData.data();
InitialData.SysMemPitch = Stride;
InitialData.SysMemSlicePitch = 0;
```

- `pSysMem`: CPU 픽셀 배열의 시작 주소
- `SysMemPitch`: CPU 배열에서 다음 행으로 이동할 바이트 수
- `SysMemSlicePitch`: 3D Texture의 다음 깊이 Slice 간격. 현재 2D Texture이므로 0

```cpp
Device->CreateTexture2D(
    &TextureDesc,
    &InitialData,
    &Image);
```

이 호출이 CPU의 PixelData를 이용하여 GPU가 사용할 Texture2D Resource를 만든다. 생성이 끝난 뒤 CPU의 `std::vector`가 파괴되어도 GPU Texture는 독립적으로 존재한다.

---

### 10. SRV가 필요한 이유

Texture2D는 실제 픽셀을 가진 Resource다. 그러나 Shader Pipeline에는 Resource 자체를 바로 연결하지 않고, 어떤 방식으로 읽을지 나타내는 View를 연결한다.

```cpp
Device->CreateShaderResourceView(
    Image,
    nullptr,
    &SRV);
```

```text
ID3D11Texture2D
→ 실제 GPU 이미지 메모리

ID3D11ShaderResourceView
→ 이 리소스를 Shader가 읽도록 해석하는 View
```

하나의 Resource는 용도와 생성 Flag가 허용한다면 서로 다른 View를 가질 수 있다. View는 전체 Resource 또는 일부 Mip/Array 범위와 읽을 Format을 지정할 수 있다.

SRV가 생성되면 내부적으로 원본 Resource에 대한 COM 참조를 유지한다. 따라서 SRV 생성이 성공한 뒤 지역 변수로 받은 `ID3D11Texture2D* Image`는 `Release`할 수 있다. 이 Release는 GPU Texture를 즉시 파괴한다는 뜻이 아니라, 지역 포인터가 가진 참조 하나를 반환한다는 뜻이다. SRV가 살아 있는 동안 원본 Resource도 살아 있다.

---

### 11. Sampler State는 텍스처를 읽는 규칙이다

SRV는 어떤 리소스를 읽을지 지정하고, Sampler는 UV로 그 리소스를 어떻게 읽을지 지정한다.

```cpp
D3D11_SAMPLER_DESC SamplerDesc = {};
SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
SamplerDesc.MinLOD = 0.0f;
SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
```

#### MIN, MAG, MIP

- MIN: Texture가 화면에서 작게 보일 때 여러 Texel을 어떻게 줄일지 결정
- MAG: Texture가 화면에서 크게 보일 때 Texel 사이를 어떻게 확대할지 결정
- MIP: 서로 다른 Mipmap Level 사이를 어떻게 선택·보간할지 결정
- LINEAR: 주변 값을 선형 보간하여 부드러운 결과 생성

현재 Texture는 Mip Level이 하나뿐이므로 MIP 보간 효과는 없지만, MIN/MAG 선형 보간은 사용된다.

#### AddressU/V/W

UV가 0~1 범위를 벗어날 때의 처리 방식이다.

```text
WRAP일 때
U = 0.2 → 0.2 위치
U = 1.2 → 다시 0.2 위치
U = 2.2 → 다시 0.2 위치
```

그래서 바닥이나 벽에 같은 텍스처를 반복할 수 있다. 2D Texture에서는 U와 V가 실제 가로·세로 방향이며 W는 3D Texture의 깊이 방향이라 현재는 실질적으로 사용하지 않는다.

#### LOD 범위

`MinLOD`와 `MaxLOD`는 사용할 수 있는 Mipmap Level 범위를 제한한다. 현재는 전체 범위를 허용하지만 Texture가 한 Level만 가지므로 0단계만 사용된다.

---

### 12. C++ 슬롯과 HLSL register 연결

Texture의 `Bind`는 SRV와 Sampler를 Pixel Shader의 슬롯에 연결한다.

```cpp
DeviceContext->PSSetShaderResources(0, 1, &SRV);
DeviceContext->PSSetSamplers(0, 1, &Sampler);
```

각 인수의 의미는 다음과 같다.

```text
0     → 0번 슬롯부터
1     → 객체 한 개
&SRV  → 연결할 SRV 포인터 배열의 시작 주소
```

HLSL에서는 같은 번호의 register로 받는다.

```hlsl
Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);
```

```text
C++ PSSetShaderResources(0, ...) ↔ HLSL register(t0)
C++ PSSetSamplers(0, ...)        ↔ HLSL register(s0)
```

`Texture2D` 변수는 이미지 전체를 HLSL 안에 복사한 변수가 아니다. C++에서 `t0`에 연결한 GPU Resource를 Shader가 참조하기 위한 핸들에 가깝다.

---

### 13. Texture.Sample의 의미

Pixel Shader는 보간된 UV를 이용하여 텍스처 색상을 조회한다.

```hlsl
float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    return DiffuseTexture.Sample(
        DiffuseSampler,
        input.texcoord);
}
```

세 요소의 역할은 다음과 같다.

```text
DiffuseTexture → 어떤 이미지에서 읽을 것인가
DiffuseSampler → 어떤 필터와 주소 규칙으로 읽을 것인가
input.texcoord → 이미지의 어느 위치를 읽을 것인가
```

`Sample`의 반환값은 RGBA를 나타내는 `float4`다.

```text
float4(R, G, B, A)
각 채널 범위는 UNORM Texture에서 0.0 ~ 1.0
```

이 값을 `SV_TARGET`으로 반환하면 Output Merger가 현재 Render Target에 해당 픽셀 색상을 기록한다.

---

### 14. 이번 단계의 Draw 직전 상태

현재 렌더링 코드는 다음 순서로 필요한 상태를 DeviceContext에 연결한다.

```cpp
ShaderSet.Bind(GetGraphics().DeviceContext);
MeshSet.Bind(GetGraphics().DeviceContext);
TextureSet.Bind(GetGraphics().DeviceContext);

GetGraphics().DeviceContext->DrawIndexed(
    MeshSet.GetIndexCount(),
    0,
    0);
```

```text
Shader Bind
→ InputLayout, Vertex Shader, Pixel Shader 선택

Mesh Bind
→ VertexBuffer, IndexBuffer 선택

Texture Bind
→ Pixel Shader t0와 s0 선택

DrawIndexed
→ 지금까지 DeviceContext에 설정한 상태를 사용하여 렌더링
```

Direct3D 11의 DeviceContext는 현재 상태를 보관하는 상태 머신이므로, 객체 생성과 현재 Draw에 사용할 객체를 Bind하는 것은 서로 다른 작업이다.

---

### 15. 발생했던 C1071 주석 오류와 UTF-8

`Texture.cpp`의 블록 주석 끝에 한글과 `*/`가 같은 줄에 있을 때 다음 오류가 발생했다.

```text
error C1071: 주석에서 예기치 않은 파일의 끝이 나타났습니다.
```

파일은 UTF-8로 저장되어 있었지만 컴파일러가 시스템 코드 페이지 949로 해석하면서 한글의 UTF-8 바이트와 뒤의 `*/`를 잘못 해석한 것이 원인이었다.

즉시 적용한 해결 방법은 주석 종료 기호를 별도 줄로 옮기는 것이었다.

```cpp
IWICFormatConverter
→ RGBA 형식으로 변환
*/
```

근본적인 해결은 컴파일러가 소스 파일을 UTF-8로 해석하도록 프로젝트에 `/utf-8` 옵션을 지정하거나 파일을 UTF-8 BOM 형식으로 저장하는 것이다. 여러 소스에서 나타난 `C4819` 경고도 같은 인코딩 문제와 관련된다.

---

### 16. 현재 구현에서 남은 개선점

현재 코드는 수동 WIC 흐름을 이해하고 텍스처를 화면에 출력하는 학습 목표를 달성했다. 다만 실제 엔진 코드로 확장하기 전에 다음을 개선해야 한다.

#### 모든 HRESULT 검사

다음 생성 함수의 반환값을 `result`에 저장하고 실패를 확인해야 한다.

```text
CreateFormatConverter
CreateTexture2D
CreateShaderResourceView
CreateSamplerState
```

현재처럼 생성 실패를 확인하지 않으면 null pointer를 다음 함수에 전달하여 접근 위반이 발생할 수 있고, 정확히 어느 단계가 실패했는지도 알기 어렵다.

#### 모든 성공·실패 경로에서 Release

지역 WIC 객체와 임시 Texture2D는 사용이 끝나면 해제해야 한다.

```text
Image(Texture2D 지역 포인터)
Converter
Frame
Decoder
ImageFactory
```

성공 경로만이 아니라 중간 함수가 실패하여 `return false`하는 경로에서도 이미 만들어진 객체를 정리해야 한다. 이후 `Microsoft::WRL::ComPtr` 또는 공통 Cleanup 경로를 사용하면 이 문제를 줄일 수 있다.

#### Initialize 반환값 사용

게임은 Texture 초기화 실패 시 렌더링을 계속하지 않아야 한다.

```cpp
if (!TextureSet.Initialize(
    GetGraphics().Device,
    L"Test.jpg"))
{
    return false;
}
```

#### 파일 경로

현재 `Test.jpg`는 실행 파일의 Working Directory에 의존하는 상대 경로다. 개발 환경과 배포 환경이 달라져도 동작하게 하려면 Asset Root를 기준으로 경로를 조합하거나 빌드 시 출력 폴더로 에셋을 복사하는 규칙이 필요하다.

#### Mipmap

현재 `MipLevels = 1`이므로 멀리 있는 Texture가 작게 보일 때 사용할 축소 이미지가 없다. 이후 DirectXTK의 WICTextureLoader 또는 DirectXTex/texconv를 이용해 Mipmap이 포함된 DDS를 준비할 수 있다.

#### 클래스 책임

현재 `Texture`는 파일 디코딩, GPU Resource 생성, Sampler 생성, Shader Bind를 모두 담당한다. 학습 단계에서는 흐름을 한곳에서 보기 좋지만 확장 단계에서는 다음처럼 나눌 수 있다.

```text
TextureLoader → 파일 해석과 CPU 픽셀 또는 GPU Texture 생성
Texture       → SRV 등 GPU Texture Resource 소유
SamplerState  → 필터/주소 규칙 소유 또는 공용 캐시
Material      → Shader + Texture + Sampler 조합
```

---

### 17. 편의 라이브러리와 직접 구현의 관계

OpenGL 프로젝트에서 사용했던 `stb_image`와 WIC는 모두 이미지 파일을 픽셀 배열로 디코딩하는 역할을 한다. `stb_image`가 OpenGL 전용인 것은 아니며, WIC도 Direct3D 전용은 아니다.

```text
stb_image 또는 WIC
→ 파일을 CPU 픽셀로 변환

OpenGL glTexImage2D 또는 Direct3D CreateTexture2D
→ CPU 픽셀을 각 그래픽 API의 GPU Resource로 생성
```

수동 WIC 구현을 한 번 완성한 이유는 편의 함수가 내부에서 하는 작업을 이해하기 위해서다. 실제 런타임 코드에서는 Microsoft의 DirectX Tool Kit `WICTextureLoader`를 이용하여 이 과정을 줄일 수 있다.

`DirectXTex`는 Texture 리사이즈, Format 변환, Mipmap 생성, BC 압축, DDS 변환 같은 오프라인 에셋 처리 도구에 더 적합하다. 최종 프로젝트에서는 PNG/JPEG를 실행 중 매번 변환하기보다 미리 Mipmap과 압축을 적용한 DDS를 준비하는 방향으로 확장할 수 있다.

---

### 18. 다음 단계: 정적 모델 로더

텍스처 출력 다음의 큰 목표는 모델 파일을 기존 `Mesh`로 변환하는 정적 모델 로더다.

```text
OBJ 또는 glTF
→ Assimp::Importer::ReadFile
→ aiScene
→ aiNode 계층 순회
→ 각 aiMesh에서 Position / Normal / UV / Index 추출
→ LightSaver의 Vertex와 Index 배열로 변환
→ Mesh::Initialize
→ Model이 여러 Mesh를 소유
→ Renderer에서 DrawIndexed
```

모델 파일 하나는 Body, Hair, Weapon처럼 여러 Mesh를 포함할 수 있으므로 `Model`은 하나의 `Mesh`가 아니라 여러 Mesh의 묶음이 된다.

```cpp
class Model
{
private:
    std::vector<Mesh> Meshes;
};
```

첫 모델 로더의 완료 기준은 다음과 같다.

```text
Position + Normal + UV + Index 읽기
→ 여러 Mesh 생성
→ Material의 기본 Texture 연결
→ 하드코딩 큐브 대신 실제 정적 모델 출력
```

그다음에는 기본 Ambient/Directional Light로 Normal을 검증하고, 프로젝트 핵심인 손전등 Spot Light를 구현한다. 이후 모듈형 맵 에셋, 여러 오브젝트의 World Transform, 충돌, 몬스터 이동·정지 판정, 발전기 순서로 게임플레이를 확장한다.

스켈레탈 애니메이션은 Bone Index/Weight, Animation Clip 보간, Bone 계층 행렬, Vertex Shader Skinning까지 필요한 별도 기능이므로 MVP 필수 범위에서는 제외한다. 손전등에 닿으면 정지하는 괴물 특성을 활용하여 먼저 정적 모델의 이동과 포즈 변화로 게임플레이를 완성하고, 일정이 남으면 Idle/Walk/Attack 세 Clip을 지원하는 방향으로 확장한다.

---

## 2026-08-19 — vcpkg와 Assimp, 정적 모델·재질·조명 로딩

### 오늘의 목표와 결과

이전 단계까지는 C++ 코드 안에 큐브의 정점과 인덱스를 직접 적었다. 이 방법은 Direct3D의 Vertex Buffer와 Index Buffer를 이해하기에는 좋지만, 실제 게임의 맵이나 괴물처럼 복잡한 모델을 작성하는 방법은 아니다. 실제 모델은 Blender 같은 도구에서 만든 뒤 OBJ, FBX, glTF 등의 파일로 저장하고, 게임 프로그램이 그 파일을 읽어 기존 `Mesh` 객체로 바꿔야 한다.

오늘은 다음 흐름을 완성했다.

```text
OBJ 모델 파일
│
├─ 정점 위치 Position
├─ 텍스처 좌표 UV
├─ 법선 Normal
├─ 면을 구성하는 Index
└─ 사용할 Material 번호
        │
        └─ MTL 파일
             └─ Diffuse Texture 경로

                ↓ Assimp가 해석

aiScene
├─ aiMesh 목록
└─ aiMaterial 목록

                ↓ LightSaver 형식으로 변환

Model
├─ ModelData[]
│   ├─ Mesh
│   └─ MaterialIndex
└─ MaterialData[]
    └─ DiffuseTexture

                ↓ Draw

Mesh Bind + 해당 Material의 Texture Bind + DrawIndexed
```

최종적으로 여러 Mesh와 여러 Material을 가진 Spider OBJ 모델을 불러와, 각 부위에 맞는 Texture를 연결하고 Directional Light를 적용하여 화면에 출력했다. Diffuse Texture가 없는 Material에는 1×1 흰색 Texture를 자동으로 생성하여 대신 사용하는 기능도 검증했다.

---

### 1. 모델 로더가 필요한 이유

GPU는 OBJ나 FBX 파일을 직접 읽지 못한다. Direct3D 11의 Input Assembler가 읽는 것은 `ID3D11Buffer`에 저장된 정점과 인덱스뿐이다.

따라서 모델 로더는 다음 사이를 연결하는 번역기다.

```text
디자이너와 모델링 프로그램이 사용하는 파일 형식
                        ↓ 번역
우리 엔진의 Vertex 배열과 Index 배열
                        ↓ Mesh::Initialize
Direct3D 11 Vertex Buffer와 Index Buffer
```

모델 로더가 렌더링을 직접 하는 것은 아니다. 파일의 데이터를 읽고, 이미 만들어 둔 `Mesh`와 `Texture`가 사용할 수 있는 형태로 변환하는 역할을 한다.

현재 클래스들의 책임은 다음과 같다.

```text
Mesh
→ 정점 버퍼와 인덱스 버퍼를 소유
→ 하나의 기하 형태를 그릴 수 있게 한다

Texture
→ 이미지에서 만든 SRV와 Sampler를 소유
→ Pixel Shader의 t0, s0 슬롯에 연결한다

MaterialData
→ Mesh를 어떤 표면으로 보이게 할지 결정하는 Texture를 소유

Model
→ 파일 하나에서 읽은 여러 Mesh와 여러 Material을 묶어서 소유
→ 각 Mesh가 어느 Material을 사용하는지 연결
```

`Model`과 `Mesh`가 따로 필요한 핵심 이유는 모델 파일 하나가 Mesh 하나라는 보장이 없기 때문이다. 예를 들어 캐릭터 파일 하나에 몸, 눈, 옷, 무기가 각각 별도의 Mesh로 들어갈 수 있다. 각 부분은 서로 다른 Texture를 사용할 수도 있다.

---

### 2. vcpkg는 무엇을 해결하는가

Assimp를 직접 사용하려면 최소한 다음 작업이 필요하다.

```text
1. Assimp 소스 또는 빌드된 라이브러리 준비
2. Header를 찾을 Include Directory 설정
3. .lib를 찾을 Library Directory 설정
4. Linker에 Assimp .lib 추가
5. 실행할 때 필요한 DLL 배치
6. Debug/Release와 x86/x64 조합 일치
```

이 작업을 프로젝트마다 수동으로 하면 경로가 컴퓨터에 종속되고, 다른 사람이 저장소를 받았을 때 같은 환경을 다시 만들기 어렵다. vcpkg는 C/C++ 라이브러리의 다운로드, 빌드, 설치, Visual Studio 연동을 관리한다.

이번 프로젝트는 Manifest Mode를 사용한다. 저장소 루트의 `vcpkg.json`이 이 프로젝트가 요구하는 외부 라이브러리 목록이다.

```json
{
  "name": "lightsaver",
  "version-string": "0.1.0",
  "dependencies": [
    "assimp"
  ],
  "builtin-baseline": "..."
}
```

각 항목의 의미는 다음과 같다.

#### `name`

Manifest 패키지의 이름이다. 현재 프로젝트를 vcpkg가 식별할 때 사용한다.

#### `version-string`

LightSaver 프로젝트 자체의 버전 문자열이다. Assimp 버전을 뜻하지 않는다.

#### `dependencies`

프로젝트가 직접 필요로 하는 라이브러리 목록이다. 현재는 `assimp` 하나를 적었지만, Assimp가 내부적으로 필요로 하는 다른 라이브러리는 vcpkg가 의존 관계를 따라 함께 설치한다.

```text
LightSaver가 assimp 요청
→ vcpkg가 assimp의 의존성 조사
→ 필요한 라이브러리를 올바른 순서로 설치
```

#### `builtin-baseline`

어느 시점의 vcpkg 포트 목록을 기준으로 패키지 버전을 결정할지 고정한다. 같은 `vcpkg.json`을 사용하더라도 기준 시점이 계속 바뀌면 오늘과 한 달 후에 서로 다른 버전이 설치될 수 있다. Baseline은 재현 가능한 빌드를 위한 기준점이다.

#### `x64-windows` Triplet

Triplet은 어떤 환경용 라이브러리를 만들지 나타내는 설정 묶음이다.

```text
x64      → 64비트 CPU 대상
windows  → Windows 대상
```

LightSaver의 빌드 플랫폼이 x64이면 Assimp도 x64로 만들어져야 한다. x64 프로그램에 x86 라이브러리를 연결할 수 없으므로 프로젝트와 vcpkg의 Triplet을 일치시켰다.

#### Visual Studio의 vcpkg 옵션

`Use Vcpkg Manifest`를 활성화하면 Visual Studio/MSBuild가 프로젝트를 빌드할 때 `vcpkg.json`을 찾고 필요한 패키지를 준비한다. 설치된 Header와 Library 경로도 빌드에 자동으로 연결된다.

이 과정은 CMake의 다음 부분과 목적이 비슷하다.

```text
find_package
target_link_libraries
include 경로 설정
```

문법과 동작 주체는 다르지만, 프로젝트가 어떤 외부 라이브러리를 필요로 하는지 선언하고 빌드 시스템에 연결한다는 목적은 같다.

#### `vcpkg_installed/`를 Git에 올리지 않는 이유

`vcpkg_installed/`에는 설치된 Header, `.lib`, DLL, 중간 빌드 결과가 들어간다. 이것은 소스 코드가 아니라 `vcpkg.json`을 보고 다시 만들 수 있는 생성물이다.

```text
Git에 저장해야 하는 것
→ vcpkg.json

각 컴퓨터에서 다시 생성되는 것
→ vcpkg_installed/
```

폴더 크기도 매우 커지고 플랫폼과 빌드 환경에 따라 내용이 달라지므로 `.gitignore`에 추가했다.

---

### 3. Assimp는 무엇을 하는가

Assimp는 Open Asset Import Library의 줄임말이다. Direct3D 11 전용 라이브러리가 아니라, 여러 3D 모델 형식을 공통 구조로 변환해 주는 파일 해석 라이브러리다.

```text
OBJ / FBX / glTF / DAE 등
             ↓ Assimp
          aiScene
```

우리는 모든 파일 형식의 문법을 직접 구현할 필요 없이 `aiScene`이라는 공통 구조만 처리하면 된다. 이후 `aiScene`의 데이터를 LightSaver의 `Vertex`, `Mesh`, `Texture` 구조로 변환한다.

```cpp
Assimp::Importer Importer;

const aiScene* Scene =
    Importer.ReadFile(FilePath, ImportFlag);
```

#### `Assimp::Importer`

Importer는 파일을 열고 해석하며, 만들어진 Scene 메모리의 수명도 관리한다. `Scene`은 Importer가 소유한 데이터를 가리키는 포인터이므로 Importer가 파괴된 뒤에는 사용하면 안 된다.

현재 코드는 `Initialize()` 안에서 Scene의 정보를 전부 자체 `Mesh`와 `Texture`로 복사한다.

```text
Importer 생성
→ ReadFile
→ Scene 데이터를 우리 객체로 복사
→ Initialize 종료
→ Importer와 Scene 파괴
→ 복사해 둔 Mesh/Texture는 계속 유효
```

이 때문에 `Scene` 포인터를 `Model` 멤버로 보관할 필요가 없다.

#### Scene 유효성 검사

```cpp
if (Scene == nullptr ||
    Scene->mRootNode == nullptr ||
    (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
{
    return false;
}
```

- `Scene == nullptr`: 파일 읽기 자체가 실패했다.
- `mRootNode == nullptr`: 모델의 계층 구조를 시작할 Root Node가 없다.
- `AI_SCENE_FLAGS_INCOMPLETE`: Scene이 불완전한 상태다.

이 검사를 통과한 뒤에만 Mesh와 Material 배열에 접근해야 한다.

---

### 4. aiScene의 전체 구조

`aiScene`은 모델 하나를 해석한 결과 전체를 담는 상자다.

```text
aiScene
├─ mRootNode
│   └─ 모델의 Node 계층 구조
├─ mMeshes[]
│   └─ 위치, UV, Normal, Face 같은 기하 데이터
├─ mMaterials[]
│   └─ Texture 경로와 재질 속성
├─ mTextures[]
│   └─ 파일 내부에 포함된 Embedded Texture
└─ mAnimations[]
    └─ Animation Clip 데이터
```

중요한 점은 Node와 Mesh가 같은 것이 아니라는 것이다.

- `aiMesh`: 실제 정점과 면 데이터
- `aiNode`: Mesh가 모델 계층에서 어디에 배치되는지 나타내는 구조

예를 들어 자동차 모델이라면 다음처럼 구성될 수 있다.

```text
CarRoot
├─ BodyNode       → BodyMesh 사용
├─ LeftWheelNode  → WheelMesh 사용 + 왼쪽 위치 변환
└─ RightWheelNode → 같은 WheelMesh 사용 + 오른쪽 위치 변환
```

Wheel Mesh의 정점 원본은 하나여도 서로 다른 Node가 다른 위치에서 사용할 수 있다.

---

### 5. Import 후처리 Flag의 의미

현재 사용한 Flag는 파일마다 제각각인 데이터를 렌더러가 기대하는 형태로 정리한다.

```cpp
aiProcess_Triangulate |
aiProcess_JoinIdenticalVertices |
aiProcess_MakeLeftHanded |
aiProcess_FlipWindingOrder |
aiProcess_FlipUVs |
aiProcess_PreTransformVertices |
aiProcess_GenSmoothNormals
```

#### `aiProcess_Triangulate`

사각형이나 그 이상의 다각형을 삼각형으로 나눈다. 현재 렌더러는 `D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST`를 사용하므로 모든 Face가 삼각형이어야 다루기 쉽다.

```text
사각형 Face {0, 1, 2, 3}
→ 삼각형 {0, 1, 2}
→ 삼각형 {0, 2, 3}
```

#### `aiProcess_JoinIdenticalVertices`

동일한 Vertex를 합쳐 Index로 재사용한다. 똑같은 Position, UV, Normal을 가진 정점을 중복 저장하는 일을 줄인다.

단, Position만 같다고 항상 같은 Vertex인 것은 아니다. 큐브 모서리처럼 위치는 같아도 면마다 UV나 Normal이 다르면 서로 다른 Vertex로 남아야 한다.

#### `aiProcess_MakeLeftHanded`

모델 데이터를 왼손 좌표계로 변환한다. 현재 LightSaver와 DirectXMath 카메라는 왼손 좌표계를 사용하므로 파일의 좌표계를 렌더러 기준에 맞춘다.

#### `aiProcess_FlipWindingOrder`

삼각형의 정점 나열 순서를 뒤집는다. 좌표계를 변환하면 앞면과 뒷면 방향도 영향을 받을 수 있으므로 현재 Rasterizer가 기대하는 Winding Order와 맞춘다.

#### `aiProcess_FlipUVs`

UV의 V축 방향을 뒤집는다. 모델 파일과 Texture API가 이미지의 위아래를 해석하는 기준 차이를 맞추기 위해 사용한다.

#### `aiProcess_GenSmoothNormals`

파일에 Normal이 없으면 주변 Face 방향을 바탕으로 부드러운 Normal을 생성한다. Normal이 없으면 현재 Diffuse Lighting의 밝기를 계산할 수 없다.

#### `aiProcess_PreTransformVertices`

Node가 가진 이동, 회전, 크기를 Vertex에 미리 적용한다.

```text
Mesh 원본 Local Vertex
× 그 Mesh를 사용하는 Node Transform
× 부모 Node Transform
→ Model Local Vertex로 미리 저장
```

여기서 결과는 아직 게임 월드 좌표가 아니다. 모델 파일 내부의 여러 부품이 서로 올바른 위치에 조립된 `Model Local` 좌표다.

렌더링할 때는 여기에 다시 오브젝트의 World 행렬을 적용한다.

```text
Mesh 원본 좌표
→ Node 계층 적용
→ Model Local 좌표
→ World 행렬 적용
→ World 좌표
→ View
→ Projection
```

이 Flag를 사용하면 직접 `mRootNode`부터 재귀 순회하며 부모 Transform을 누적하는 코드를 당장 작성하지 않아도 된다. 정적 모델을 빠르게 출력하기에 적합하다.

대신 Node 계층이 Vertex에 구워져 평평해지므로 Node를 따로 움직이거나 Bone Animation을 처리하기에는 맞지 않는다. 현재 MVP는 정적 모델이 목표이므로 이 선택이 적절하다. 나중에 Animation을 구현한다면 이 Flag를 제거하고 Node/Bone 계층을 직접 보존해야 한다.

---

### 6. OBJ, MTL, 이미지 파일의 관계

OBJ 모델 하나가 완성되어 보이려면 보통 세 종류의 파일이 함께 필요하다.

```text
model.obj
→ Position, UV, Normal, Face
→ 어느 Material을 사용할지 기록

model.mtl
→ Material의 이름과 색상 속성
→ Diffuse Texture 파일 경로

texture.png 또는 texture.jpg
→ 실제 픽셀 이미지
```

OBJ의 다음 줄은 사용할 MTL 파일을 지정한다.

```obj
mtllib spider.mtl
```

다음 줄은 이후 Face들이 특정 Material을 사용한다고 지정한다.

```obj
usemtl Skin
```

MTL에서는 같은 이름의 Material을 정의한다.

```mtl
newmtl Skin
map_Kd .\wal67ar_small.jpg
```

`map_Kd`는 Diffuse Color에 사용할 Texture 경로다.

```text
OBJ의 usemtl Skin
→ MTL의 newmtl Skin 찾기
→ map_Kd 경로 확인
→ JPG를 Texture로 로딩
```

OBJ만 있고 MTL이나 이미지가 없으면 기하 형태는 읽을 수 있어도 원래 표면은 재현되지 않는다. 반대로 이미지 파일만 있어도 어떤 Mesh의 어느 UV에 사용할지 알 수 없다.

Spider 모델은 여러 Material을 사용하므로 몸통, 다리, 눈 등의 부위가 서로 다른 JPG를 참조한다. 이를 통해 여러 Mesh/Material 연결이 실제로 작동하는지 확인했다.

---

### 7. `ProcessMesh()`가 aiMesh를 우리 Mesh로 바꾸는 과정

`aiMesh`는 Assimp의 형식이고 `Mesh`는 LightSaver의 형식이다. `ProcessMesh()`는 둘 사이를 변환한다.

#### Vertex 배열 준비

```cpp
std::vector<Vertex> SourceVertices;
SourceVertices.reserve(SourceMesh->mNumVertices);
```

`reserve()`는 앞으로 필요한 메모리 용량을 미리 확보하지만 Vector의 원소 개수는 늘리지 않는다.

```text
size     = 0
capacity = mNumVertices
```

이후 `push_back()`으로 Vertex를 한 개씩 추가한다.

```cpp
SourceVertices.push_back(NewVertex);
```

여기서 `resize(mNumVertices)`를 한 뒤 다시 `push_back()`하면 기본 Vertex가 먼저 mNumVertices개 생기고 그 뒤에 실제 Vertex가 추가되어 원소 수가 두 배가 된다. 이번 구조에서는 `reserve + push_back` 조합이 맞다.

#### Position 복사

```cpp
NewVertex.x = SourceMesh->mVertices[i].x;
NewVertex.y = SourceMesh->mVertices[i].y;
NewVertex.z = SourceMesh->mVertices[i].z;
```

`mVertices[i]`는 i번 Vertex의 Position이다.

#### UV 복사

```cpp
if (SourceMesh->HasTextureCoords(0))
{
    NewVertex.u = SourceMesh->mTextureCoords[0][i].x;
    NewVertex.v = SourceMesh->mTextureCoords[0][i].y;
}
```

`mTextureCoords`는 먼저 UV 채널을 고르고, 그다음 Vertex를 고르는 2단 구조다.

```text
mTextureCoords[0][i]
               │  └─ i번 Vertex
               └──── 0번 UV 채널
```

대부분의 기본 모델은 UV 채널 0을 사용한다. 두 번째 UV 채널은 Lightmap처럼 별도 좌표가 필요할 때 사용할 수 있다.

#### Normal 복사

```cpp
if (SourceMesh->HasNormals())
{
    NewVertex.nx = SourceMesh->mNormals[i].x;
    NewVertex.ny = SourceMesh->mNormals[i].y;
    NewVertex.nz = SourceMesh->mNormals[i].z;
}
```

Normal은 표면이 어느 방향을 향하는지 나타내는 방향 벡터다. Position처럼 표면의 위치를 뜻하지 않는다. 조명이 표면을 정면으로 비추는지 옆에서 비추는지 판단할 때 사용한다.

#### Face와 Index 복사

```cpp
for (UINT i = 0; i < SourceMesh->mNumFaces; ++i)
{
    for (UINT j = 0;
         j < SourceMesh->mFaces[i].mNumIndices;
         ++j)
    {
        SourceIndices.push_back(
            SourceMesh->mFaces[i].mIndices[j]);
    }
}
```

`mFaces[i]`는 i번째 면이고, `mIndices[j]`는 그 면을 구성하는 Vertex 번호다. `Triangulate`를 적용했으므로 정상적인 Face는 보통 Index 세 개를 가진다.

```text
Face 0 → {0, 1, 2}
Face 1 → {2, 1, 3}
```

완성된 배열은 기존 `Mesh::Initialize()`로 전달한다.

```text
Assimp CPU 데이터
→ std::vector<Vertex>, std::vector<UINT>
→ Mesh::Initialize
→ ID3D11Buffer VertexBuffer
→ ID3D11Buffer IndexBuffer
```

이 구조의 장점은 모델 파일 형식이 바뀌어도 GPU Buffer를 만드는 `Mesh` 코드는 바뀌지 않는다는 것이다.

---

### 8. Vertex에 Normal을 추가하면서 바뀐 Input Layout

현재 Vertex의 메모리 구조는 다음과 같다.

```cpp
struct Vertex
{
    float x, y, z;       // 12 bytes
    float u, v;          //  8 bytes
    float nx, ny, nz;    // 12 bytes
};
```

총 크기는 32바이트다.

```text
Offset 0                 Position float3
Offset 12                UV       float2
Offset 20                Normal   float3
끝 Offset 32             다음 Vertex 시작
```

Input Layout도 이 실제 메모리 배치와 정확히 일치해야 한다.

```cpp
POSITION → R32G32B32_FLOAT, Offset 0
TEXCOORD → R32G32_FLOAT,    Offset 12
NORMAL   → R32G32B32_FLOAT, Offset 20
```

HLSL 입력도 같은 Semantic을 요구한다.

```hlsl
struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
};
```

이 중 하나라도 Format, Offset, Semantic이 틀리면 GPU는 바이트를 잘못 해석한다. 예를 들어 Normal Offset을 12로 지정하면 UV의 바이트부터 Normal이라고 읽게 된다.

---

### 9. 여러 Mesh와 Material을 연결하는 `MaterialIndex`

Scene에는 Mesh 배열과 Material 배열이 따로 있다.

```text
Scene->mMeshes[]
Scene->mMaterials[]
```

각 `aiMesh`의 `mMaterialIndex`는 Material 배열의 몇 번째 항목을 사용할지 알려 준다.

```cpp
NewModelData.MaterialIndex =
    SourceMesh->mMaterialIndex;
```

LightSaver도 같은 관계를 보존한다.

```cpp
struct ModelData
{
    std::unique_ptr<Mesh> MeshData;
    UINT MaterialIndex;
};

struct MaterialData
{
    std::unique_ptr<Texture> DiffuseTexture;
};
```

예를 들어 다음과 같다면:

```text
ModelData[0].MaterialIndex = 2
```

0번 Mesh를 그릴 때 `MaterialDatas[2]`의 Texture를 사용한다.

```cpp
MaterialDatas[ModelData.MaterialIndex]
    .DiffuseTexture->Bind(DeviceContext);
```

Mesh와 Material을 인덱스로 연결하면 같은 Material을 여러 Mesh가 공유할 수도 있다.

---

### 10. `unique_ptr`, `std::move`, 소유권

`Mesh`와 `Texture`는 내부에 COM Resource 포인터를 소유한다. 이 객체가 무심코 복사되면 두 객체가 같은 COM 포인터를 해제하여 이중 해제 문제가 생길 수 있다. 현재는 `std::unique_ptr`로 한 객체의 소유자가 하나뿐임을 표현한다.

```cpp
auto NewMesh = std::make_unique<Mesh>();
```

```text
NewMesh
→ 새 Mesh 객체를 유일하게 소유
```

Vector에 넣을 때는 복사하지 않고 소유권을 이동한다.

```cpp
NewModelData.MeshData = std::move(NewMesh);
```

이후 상태는 다음과 같다.

```text
이동 전
NewMesh ─────→ Mesh

이동 후
NewMesh       → nullptr
MeshData ─────→ Mesh
```

`std::move`가 객체 메모리 자체를 물리적으로 복사한다는 뜻은 아니다. 이 값을 이제 다른 소유자에게 넘겨도 된다는 이동 가능 상태로 변환한다. `unique_ptr`의 경우 내부 포인터의 소유권이 전달된다.

`ModelDatas.clear()`와 `MaterialDatas.clear()`는 같은 `Model` 객체에 다른 파일을 다시 로딩할 때 이전 Mesh와 Texture를 먼저 정리하기 위해 사용한다. `unique_ptr`가 Vector 안에 있으므로 `clear()`할 때 각 객체의 소멸자가 자동 호출된다.

---

### 11. Texture 경로를 모델 파일 기준으로 조합하는 이유

MTL의 Texture 경로는 대개 모델 파일이 있는 폴더를 기준으로 한 상대 경로다.

```mtl
map_Kd .\SpiderTex.jpg
```

프로그램의 현재 Working Directory에 `SpiderTex.jpg`가 있는 것은 아니므로 그대로 WIC에 넘기면 파일을 찾지 못할 수 있다.

```cpp
std::filesystem::path ModelPath = FilePath;

std::filesystem::path FullTexturePath =
    ModelPath.parent_path() /
    TexturePath.C_Str();
```

예:

```text
ModelPath
= Assets/Models/Spider/spider.obj

ModelPath.parent_path()
= Assets/Models/Spider

TexturePath
= ./SpiderTex.jpg

결과
= Assets/Models/Spider/SpiderTex.jpg
```

`std::filesystem::path`를 사용하면 문자열을 직접 이어 붙이는 것보다 경로 구분자와 상대 경로 처리를 명확하게 표현할 수 있다.

---

### 12. Model의 실제 Draw 흐름

Model은 자신이 가진 모든 Mesh를 순회한다.

```cpp
for (const auto& ModelData : ModelDatas)
{
    ModelData.MeshData->Bind(DeviceContext);

    MaterialDatas[ModelData.MaterialIndex]
        .DiffuseTexture->Bind(DeviceContext);

    DeviceContext->DrawIndexed(
        ModelData.MeshData->GetIndexCount(),
        0,
        0);
}
```

Mesh 하나마다 다음 상태를 새로 선택한다.

```text
1. 이 부위의 Vertex/Index Buffer Bind
2. 이 부위가 참조하는 Material의 Texture Bind
3. DrawIndexed
```

Model에 Mesh가 다섯 개면 일반적으로 DrawIndexed도 다섯 번 호출된다. `DrawIndexed` 한 번이 모델 파일 전체를 자동으로 그리는 것이 아니다.

현재 전체 프레임 흐름은 다음과 같다.

```text
LightSaverGame::Render
├─ CameraBuffer 갱신
├─ ObjectBuffer 갱신
├─ LightBuffer 갱신
├─ Render Target / Depth / Viewport 설정
├─ Shader Bind
└─ Model::Draw
    ├─ Mesh 0 + Material Texture Bind → DrawIndexed
    ├─ Mesh 1 + Material Texture Bind → DrawIndexed
    └─ ...
```

---

### 13. Normal은 왜 World 변환이 필요한가

모델 파일의 Normal은 Model Local 공간에 있다. 모델이 회전했는데 Normal은 회전하지 않으면 화면에 보이는 표면 방향과 조명 계산에 사용하는 방향이 서로 달라진다.

따라서 Vertex Shader에서 Normal도 World 방향으로 변환한다.

```hlsl
output.normal =
    mul(input.normal, (float3x3)World);
```

`World`는 4×4 행렬이다. 그 안에는 이동, 회전, 크기 정보가 들어 있다.

```text
4×4 World Matrix
┌                 ┐
│ 회전/크기  ...  │
│ 회전/크기  ...  │
│ 회전/크기  ...  │
│ 이동 x y z  1   │
└                 ┘
```

이를 `(float3x3)`으로 변환하면 왼쪽 위 3×3 부분만 사용한다. 이동 정보가 있는 마지막 행/열이 제외된다.

Normal은 위치가 아니라 방향이므로 이동시키면 안 된다.

```text
Position은 모델이 이동하면 함께 이동해야 함
Normal은 모델이 이동해도 방향이 바뀌지 않음
```

다만 `(float3x3)World`만 사용하는 방식은 회전과 균일 Scale에는 적합하지만 비균일 Scale에서는 Normal이 표면과 수직을 유지하지 못할 수 있다.

```text
균일 Scale     (0.01, 0.01, 0.01) → 현재 방식 사용 가능
비균일 Scale   (2.0, 1.0, 0.5)    → 역전치 Normal Matrix 필요
```

현재 Spider는 세 축에 모두 `0.01`을 적용하므로 균일 Scale이다.

---

### 14. Ambient Light와 Diffuse Light

현재 Pixel Shader는 Texture 색상에 두 종류의 빛을 더한다.

#### Ambient Light

주변광은 빛의 방향과 관계없이 전체 표면에 최소 밝기를 준다.

```hlsl
float3 AmbientLight =
    TextureColor.rgb * AmbientStrength;
```

주변광이 전혀 없으면 주광원이 닿지 않는 면이 완전한 검은색이 된다. 실제 간접광을 계산한 것은 아니고, 아직 Shadow나 Global Illumination이 없는 단계에서 최소한의 밝기를 단순하게 더한 것이다.

#### Diffuse Light

Diffuse는 빛이 표면을 얼마나 정면으로 비추는지에 따라 밝기가 달라지는 난반사 조명이다.

```hlsl
float3 Normal = normalize(input.normal);
float3 ToLight = normalize(ToLightDirection);

float Diffuse =
    saturate(dot(Normal, ToLight));
```

내적의 결과는 두 방향이 얼마나 같은지 나타낸다.

```text
Normal과 ToLight가 같은 방향      → dot =  1 → 가장 밝음
두 방향이 90도                    → dot =  0 → Diffuse 없음
빛이 표면 뒤쪽에 있음             → dot < 0 → 0으로 제한
```

`saturate(x)`는 값을 0과 1 사이로 제한한다.

```text
saturate(-0.4) = 0
saturate( 0.6) = 0.6
saturate( 1.2) = 1
```

최종 Diffuse 색상은 다음 요소를 곱한다.

```hlsl
float3 DiffuseLight =
    TextureColor.rgb *
    LightColor *
    Diffuse *
    DiffuseStrength;
```

- `TextureColor`: 표면 원래 색
- `LightColor`: 빛 자체의 색
- `Diffuse`: 각도에 따른 밝기
- `DiffuseStrength`: 빛의 전체 강도 조절

최종 출력은 Ambient와 Diffuse를 더한 값이다.

```hlsl
return float4(
    AmbientLight + DiffuseLight,
    TextureColor.a);
```

현재 `ToLightDirection = (0, 0, -1)`은 표면에서 빛을 향하는 방향이 -Z라는 의미로 사용한다. 변수 이름이 `ToLightDirection`이므로 빛이 진행하는 방향이 아니라 표면에서 광원을 바라보는 방향이라는 점을 구분해야 한다.

---

### 15. Light Constant Buffer와 16바이트 정렬

CPU 구조체와 HLSL cbuffer는 같은 메모리 배치를 가져야 한다.

```cpp
struct alignas(16) LightBufferData
{
    DirectX::XMFLOAT3 ToLightDirection;
    float AmbientStrength;

    DirectX::XMFLOAT3 LightColor;
    float DiffuseStrength;
};
```

```text
첫 16 bytes
float3 ToLightDirection 12 bytes
float  AmbientStrength   4 bytes

다음 16 bytes
float3 LightColor       12 bytes
float  DiffuseStrength   4 bytes

총 32 bytes
```

HLSL도 같은 순서로 선언한다.

```hlsl
cbuffer LightBuffer : register(b2)
{
    float3 ToLightDirection;
    float AmbientStrength;

    float3 LightColor;
    float DiffuseStrength;
}
```

CPU에서 `Map(D3D11_MAP_WRITE_DISCARD)`으로 이번 프레임의 값을 기록하고 `Unmap()`한 뒤 Pixel Shader의 b2 슬롯에 연결한다.

```cpp
DeviceContext->PSSetConstantBuffers(
    2, 1, &LightBuffer);
```

```text
C++ StartSlot 2
↕
HLSL register(b2)
```

`LightBuffer`를 Vertex Shader에도 연결했지만 현재 조명 계산은 Pixel Shader에서만 수행하므로 실제로 읽는 곳은 PS의 b2다. 앞으로 VS에서 사용하지 않는다면 PS에만 Bind해도 된다.

---

### 16. Diffuse Texture가 없는 Material과 기본 흰색 Texture

모든 Material이 `map_Kd`를 가진다는 보장은 없다. Texture가 없는 Material에서 다음 코드를 무조건 실행하면 `DiffuseTexture`가 null일 수 있다.

```cpp
DiffuseTexture->Bind(DeviceContext);
```

이를 막기 위해 Texture가 없는 Material에는 1×1 흰색 Texture를 만든다.

```cpp
NewTexture->InitializeByColor(
    Device,
    255, 255, 255, 255);
```

흰색을 사용하는 이유는 현재 Shader가 Texture 색상과 Lighting을 곱하기 때문이다.

```text
흰색 (1, 1, 1) × 조명색
→ 조명색을 그대로 유지
```

검은색 기본 Texture를 쓰면 모든 조명 결과에 0이 곱해져 검게 보인다. 따라서 Texture가 없는 표면을 조명으로 확인하려면 흰색이 중립적인 기본값이다.

#### `DXGI_FORMAT_R8G8B8A8_UNORM`

```text
R 8 bits
G 8 bits
B 8 bits
A 8 bits
합계 32 bits = 4 bytes per pixel
```

`UNORM`은 Unsigned Normalized를 뜻한다. CPU 메모리의 0~255 정수 값을 Shader에서 0.0~1.0 실수로 읽는다.

```text
CPU 0   → Shader 0.0
CPU 128 → Shader 약 0.502
CPU 255 → Shader 1.0
```

따라서 다음 배열은 완전히 불투명한 흰색 픽셀 하나다.

```cpp
unsigned char PixelData[4] =
{
    255, 255, 255, 255
};
```

#### 1×1 Texture Descriptor

```cpp
TextureDesc.Width = 1;
TextureDesc.Height = 1;
TextureDesc.MipLevels = 1;
TextureDesc.ArraySize = 1;
TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
```

- 크기 1×1: 모든 UV에서 같은 한 픽셀을 사용한다.
- MipLevels 1: 축소 단계가 필요 없는 단일 픽셀이다.
- Immutable: 생성 뒤 픽셀 색을 변경하지 않는다.
- Shader Resource: Pixel Shader에서 Sample할 수 있어야 한다.

#### `SysMemPitch = 4`

Pitch는 이미지 한 행의 바이트 수다.

```text
Width 1 pixel × RGBA 4 bytes = 한 행 4 bytes
```

따라서 1×1 RGBA Texture의 Pitch는 4다.

#### SRV는 색상이 아니라 Texture를 보는 View다

SRV 안에 흰색 값 자체를 직접 넣는 것은 아니다.

```text
PixelData[4]
→ ID3D11Texture2D 생성
→ 그 Texture를 가리키는 SRV 생성
→ PSSetShaderResources로 t0에 Bind
```

파일에서 읽은 Texture와 기본 색 Texture는 만들어지는 출발점만 다르다.

```text
파일 Texture: WIC가 만든 RGBA 배열 → Texture2D → SRV
기본 Texture: 직접 만든 RGBA 4 bytes → Texture2D → SRV
```

그 이후 Shader가 보는 것은 둘 다 똑같은 `Texture2D`와 SRV다. 그래서 Draw 코드에 별도의 Shader 분기가 필요 없다.

---

### 17. 기본 Texture 테스트에서 발생한 nullptr 오류

Spider의 `Skin` Material에서 `map_Kd`를 잠시 제거하여 Texture가 없는 경로를 시험했다. 처음에는 `Texture::Bind()`에서 접근 위반이 발생했다.

디버거에서 중요한 값은 다음이었다.

```text
this = nullptr
```

이것은 `SRV`가 null이라는 뜻이 아니라, `Bind()`를 호출한 `Texture` 객체 자체가 존재하지 않는다는 뜻이다.

문제가 된 흐름은 다음과 같았다.

```cpp
auto NewTexture = std::make_unique<Texture>();

if (Texture가 없음)
{
    NewTexture->InitializeByColor(...);
    continue;
}

MaterialDatas[i].DiffuseTexture =
    std::move(NewTexture);
```

기본 Texture는 `NewTexture` 안에 정상 생성됐지만, `MaterialDatas[i]`로 소유권을 옮기기 전에 `continue`로 다음 반복으로 넘어갔다.

```text
NewTexture 생성
→ 흰색 SRV 생성
→ continue
→ 지역 unique_ptr 파괴
→ 흰색 Texture도 파괴
→ MaterialDatas[i].DiffuseTexture는 여전히 nullptr
```

수정한 흐름은 다음과 같다.

```text
NewTexture 생성
→ 파일 Texture 또는 흰색 Texture로 초기화
→ MaterialDatas[i].DiffuseTexture에 std::move
→ 다음 반복
```

테스트 결과 `Skin`을 사용하는 몸통 윗부분만 흰색으로 출력되고 나머지 부위는 기존 JPG Texture를 사용했다. 이것으로 다음 세 관계가 모두 정상임을 확인했다.

```text
Mesh의 MaterialIndex 연결 정상
Texture가 없는 Material 감지 정상
InitializeByColor의 SRV Bind 정상
```

검증 뒤 `spider.mtl`의 원래 `map_Kd`를 복구했다.

---

### 18. 임시 Texture2D와 COM 수명

SRV를 만들기 위해 지역 `ID3D11Texture2D* Image`가 필요하다.

```cpp
ID3D11Texture2D* Image = nullptr;

Device->CreateTexture2D(
    &TextureDesc,
    &InitialData,
    &Image);

Device->CreateShaderResourceView(
    Image,
    nullptr,
    &SRV);
```

SRV 생성이 성공하면 SRV가 Texture Resource에 필요한 COM 참조를 유지한다. 따라서 지역 포인터의 참조는 해제해야 한다.

```cpp
Image->Release();
Image = nullptr;
```

이것은 GPU Texture를 즉시 없애는 것이 아니다.

```text
CreateTexture2D 후
지역 Image 포인터가 참조 1개 소유

CreateShaderResourceView 후
SRV가 Texture에 필요한 참조 추가

지역 Image Release 후
지역 참조만 제거
SRV가 참조하므로 Texture는 계속 존재
```

`Release()`하지 않으면 지역 변수는 함수 종료로 사라져도 COM 참조 횟수는 줄지 않아 Resource 누수가 발생한다.

현재 `InitializeByColor()`는 학습 기능을 우선 완성한 상태다. 다음 리팩터링에서는 `CreateTexture2D`, `CreateShaderResourceView`, `CreateSamplerState`의 모든 `HRESULT`를 검사하고, 실패한 중간 단계에서도 이미 생성한 Resource를 정리해야 한다. `Microsoft::WRL::ComPtr`를 사용하면 여러 조기 반환 경로의 수동 `Release()`를 줄일 수 있다.

---

### 19. `.gitignore`의 `*.obj` 충돌

C++ 컴파일러도 중간 산출물로 `.obj` 파일을 만들고, 3D 모델 형식도 `.obj` 확장자를 사용한다.

기존 규칙은 모든 `.obj` 파일을 무시했다.

```gitignore
*.obj
```

이 규칙만 있으면 C++ 빌드 산출물뿐 아니라 다음 실제 모델도 Git에서 사라진다.

```text
Assets/Models/Spider/spider.obj
Assets/Models/Test.obj
```

따라서 전체 `.obj` 무시 규칙 뒤에 에셋 폴더의 예외를 추가했다.

```gitignore
*.obj
!LightSaver/Assets/**/*.obj
```

Git ignore 규칙은 뒤쪽 규칙이 앞쪽 규칙을 다시 뒤집을 수 있다. 이제 프로젝트 곳곳의 컴파일 산출물 `.obj`는 계속 무시하면서, `Assets` 아래의 3D 모델 OBJ만 저장소에 포함한다.

---

### 20. 현재 정적 Model Loader의 완료 범위

현재 지원하는 기능은 다음과 같다.

```text
여러 aiMesh 읽기
Position / UV / Normal 읽기
Face Index 읽기
여러 aiMaterial 읽기
Mesh의 MaterialIndex 보존
Diffuse Texture 0번 읽기
모델 폴더 기준 Texture 경로 조합
Texture가 없을 때 1×1 흰색 대체
Node Transform을 Vertex에 미리 적용
Ambient + Directional Diffuse Lighting
```

아직 지원하지 않는 항목은 다음과 같다.

```text
Material당 여러 Diffuse Texture
Normal Map
Metallic / Roughness PBR Material
Embedded Texture
투명 Material과 Blend State
Node 계층을 보존한 개별 부품 Transform
Bone / Skeletal Animation
Resource 중복 로딩 방지와 캐시
여러 Model Instance의 개별 World Transform 관리
```

MVP 공포 게임을 위해 이 기능을 전부 먼저 만들 필요는 없다. 현재 정적 모델을 Texture와 조명까지 포함하여 출력할 수 있으므로 모델 로더의 첫 번째 완료 기준을 달성했다.

---

### 21. 다음 단계: 손전등으로 이어지는 조명 확장

현재 Directional Light는 위치가 없고 모든 픽셀에서 같은 빛 방향을 사용한다. 태양처럼 매우 멀리 있는 광원을 단순화한 형태다.

게임의 핵심인 손전등은 위치와 방향, 범위, 원뿔 각도가 필요하다. 바로 Spot Light 공식을 한 번에 작성하기보다 다음 순서로 확장한다.

```text
1. Vertex Shader에서 World Position을 Pixel Shader로 전달

2. Point Light
   픽셀에서 광원까지의 방향 계산
   거리에 따라 밝기 감소

3. Spot Light
   광원의 Forward와 픽셀 방향의 내적
   원뿔 안쪽만 밝게 처리
   안쪽/바깥쪽 각도로 경계를 부드럽게 처리

4. 카메라 연결
   Light Position  = Camera Position
   Light Direction = Camera Forward

5. 게임플레이 판정
   Monster가 손전등 거리와 원뿔 안에 있는지 CPU에서도 검사
   범위 안이면 이동 정지
```

다음 과제는 `PS_INPUT`에 World Position을 추가하는 것이다. Pixel Shader가 각 픽셀의 월드 위치를 알아야 `LightPosition - WorldPosition`으로 광원 방향과 거리를 계산할 수 있다.

---

### 오늘의 핵심 요약

```text
vcpkg.json
→ 프로젝트가 Assimp에 의존한다는 재현 가능한 선언

Assimp
→ 여러 모델 파일을 aiScene 공통 구조로 해석

aiMesh
→ Position, UV, Normal, Face/Index

aiMaterial
→ Diffuse Texture 같은 표면 정보

mMaterialIndex
→ 특정 Mesh와 특정 Material을 연결

Model
→ 여러 Mesh와 Material을 함께 소유하고 순서대로 Draw

Normal + Light Direction의 내적
→ 표면이 빛을 얼마나 정면으로 받는지 계산

1×1 흰색 Texture
→ 실제 Diffuse Texture가 없는 Material의 안전한 기본값

unique_ptr + std::move
→ Mesh와 Texture의 소유자를 하나로 유지

PreTransformVertices
→ Node 계층 변환을 Vertex에 미리 적용한 정적 모델용 단순화
```

이번 단계에서 가장 중요한 구조적 변화는 하드코딩한 큐브를 그리던 렌더러가 외부 모델 파일을 받아 여러 Mesh와 Material을 처리할 수 있게 되었다는 점이다. 앞으로 맵, 발전기, 괴물 모델을 바꾸더라도 GPU Buffer 생성과 Draw의 기본 구조를 다시 작성할 필요가 없다.

---

## 2026-08-20 — Point Light에서 카메라 손전등 Spot Light까지

### 오늘의 목표와 결과

기존 조명은 모든 픽셀이 같은 방향의 빛을 받는 Directional Light였다. 오늘은 여기에 위치, 거리, 범위, 원뿔 방향과 각도를 추가하여 카메라에 붙어 움직이는 Spot Light로 확장했다.

구현 흐름은 다음과 같다.

```text
Directional Light
빛의 방향만 존재
        ↓
Pixel Shader에 World Position 전달
각 픽셀이 월드의 어디에 있는지 확보
        ↓
Point Light
픽셀마다 광원 방향과 거리 계산
        ↓
Distance Attenuation
멀어질수록 직접광 감소
        ↓
Spot Light
광원의 진행 방향과 픽셀 방향의 각도 검사
        ↓
Camera 연결
LightPosition  = Camera Position
SpotDirection = Camera Forward
```

현재 결과는 카메라가 움직이면 광원 위치도 같이 움직이고, 마우스로 카메라를 회전하면 Spot Light 원뿔도 화면 중앙을 따라 회전하는 손전등의 기본 형태다.

---

### 1. Directional Light만으로 손전등을 만들 수 없는 이유

Directional Light는 광원이 무한히 멀리 있다고 가정한다. 그래서 모든 픽셀에서 빛의 방향이 같다.

```hlsl
float3 ToLightDir = normalize(ToLightDirection);
```

이 방식에는 다음 정보가 없다.

```text
광원의 위치
픽셀과 광원 사이의 거리
빛이 도달하는 최대 범위
빛이 퍼지는 원뿔의 방향
원뿔의 안쪽 각도와 바깥쪽 각도
```

태양처럼 넓은 공간에 평행하게 들어오는 빛에는 적합하지만, 플레이어 손에 붙어 좁은 범위를 비추는 손전등에는 적합하지 않다.

손전등을 만들려면 Pixel Shader가 적어도 다음 값을 알아야 한다.

```text
현재 픽셀의 월드 위치
손전등의 월드 위치
손전등이 향하는 월드 방향
손전등의 최대 거리
손전등 원뿔의 각도
```

---

### 2. Pixel Shader에 World Position이 필요한 이유

기존 Pixel Shader는 Texture 좌표와 Normal만 받았다. 그러나 광원의 위치가 생기면 각 픽셀에서 광원까지의 방향이 서로 달라진다.

```text
왼쪽에 있는 픽셀 → 광원은 오른쪽 위에 있음
오른쪽에 있는 픽셀 → 광원은 왼쪽 위에 있음
광원과 가까운 픽셀 → 거리가 짧음
광원과 먼 픽셀 → 거리가 김
```

따라서 Vertex Shader에서 계산한 World Position을 Pixel Shader로 전달했다.

```hlsl
struct PS_INPUT
{
    float4 position      : SV_POSITION;
    float3 worldPosition : POSITION1;
    float2 texcoord      : TEXCOORD;
    float3 normal        : NORMAL;
};
```

Vertex Shader의 위치 변환은 다음 순서다.

```hlsl
float4 localPosition = float4(input.position, 1.0f);
float4 worldPosition = mul(localPosition, World);
float4 viewPosition = mul(worldPosition, View);
output.position = mul(viewPosition, Projection);

output.worldPosition = worldPosition.xyz;
```

두 출력은 목적이 다르다.

```text
output.position
→ Rasterizer가 화면에서 삼각형과 픽셀 위치를 결정하는 최종 Clip Position

output.worldPosition
→ 조명 계산을 위해 Pixel Shader에 전달하는 World Position
```

Rasterizer는 삼각형 세 정점의 `worldPosition`을 각 픽셀까지 보간한다. 따라서 Pixel Shader는 픽셀마다 서로 다른 월드 위치를 받는다.

---

### 3. 위치 두 개를 빼면 방향과 거리를 얻을 수 있다

광원 위치를 `L`, 현재 픽셀 위치를 `P`라고 하자.

```text
L = LightPosition
P = input.worldPosition
```

픽셀에서 광원으로 향하는 벡터는 다음과 같다.

```text
L - P
```

코드에서는 다음과 같다.

```hlsl
float3 PixelToLight = LightPosition - input.worldPosition;
```

예를 들어:

```text
P = (1, 0, 2)
L = (1, 0, 7)

L - P
= (1, 0, 7) - (1, 0, 2)
= (0, 0, 5)
```

이 벡터에는 두 정보가 동시에 들어 있다.

```text
방향: +Z
길이: 5
```

`length()`는 벡터의 길이, 즉 거리를 구한다.

```hlsl
float ToLightDistance = length(PixelToLight);
```

3차원 벡터 `(x, y, z)`의 길이는 피타고라스 정리를 3차원으로 확장한 것이다.

```text
|v| = sqrt(x² + y² + z²)
```

방향만 필요할 때는 `normalize()`를 사용한다.

```hlsl
float3 ToLightDir = normalize(PixelToLight);
```

정규화는 방향을 유지하면서 길이를 1로 만든다.

```text
normalize(v) = v / |v|
```

거리 계산은 정규화 전에 해야 한다. 정규화한 벡터는 길이가 언제나 1이므로 원래 거리를 잃어버리기 때문이다.

---

### 4. Point Light의 Diffuse 계산

Directional Light에서는 모든 픽셀이 같은 빛 방향을 사용했다. Point Light에서는 픽셀마다 다음 방향을 다시 계산한다.

```hlsl
float3 ToLightDir =
    normalize(LightPosition - input.worldPosition);
```

표면 Normal과 빛 방향을 모두 정규화하면 내적은 두 방향 사이 각도의 코사인이 된다.

```text
dot(N, L) = |N||L|cos(theta)

|N| = 1, |L| = 1이면
dot(N, L) = cos(theta)
```

각도별 값은 다음과 같다.

```text
theta =   0도 → cos =  1 → 빛을 정면으로 받음
theta =  60도 → cos = 0.5 → 절반 정도 기울어짐
theta =  90도 → cos =  0 → 빛이 표면을 스쳐 지나감
theta = 180도 → cos = -1 → 빛이 표면 뒤쪽에 있음
```

음수는 표면 뒤쪽의 빛이므로 `saturate()`로 0에서 1 사이로 제한한다.

```hlsl
float Diffuse = saturate(dot(Normal, ToLightDir));
```

`saturate(x)`는 다음과 같다.

```text
x < 0이면 0
0 <= x <= 1이면 x
x > 1이면 1
```

이 단계까지 구현하면 위치를 가진 Point Light가 된다. 아직 거리와 원뿔 범위는 적용하지 않은 상태다.

---

### 5. 거리 감쇠 Distance Attenuation

실제 빛은 광원에서 멀어질수록 약해진다. 이번 구현에서는 이해하기 쉬운 선형 감쇠를 사용했다.

```hlsl
float DistanceAttenuation =
    saturate(1.0f - ToLightDistance / LightRange);
```

`LightRange = 10`일 때 결과는 다음과 같다.

```text
거리 0  → 1 -  0/10 = 1.0
거리 2  → 1 -  2/10 = 0.8
거리 5  → 1 -  5/10 = 0.5
거리 10 → 1 - 10/10 = 0.0
거리 15 → 1 - 15/10 = -0.5 → saturate 후 0.0
```

그래프로 생각하면 광원 위치에서 밝기 1로 시작해 `LightRange`에서 0이 되는 직선이다.

```text
밝기
1.0 |\
    | \
    |  \
0.0 |___\________ 거리
       LightRange
```

이 식은 배우고 조절하기 쉽지만 물리적으로 정확한 빛은 아니다. 물리적인 빛의 세기는 대체로 거리 제곱에 반비례한다.

```text
Intensity ∝ 1 / distance²
```

하지만 거리가 0에 가까울 때 값이 지나치게 커지는 문제와 게임에서 조절하기 어려운 문제가 있으므로, 현재 단계에서는 명확한 최대 범위를 갖는 선형 감쇠를 사용했다.

중요한 점은 거리 감쇠를 Ambient에 곱하지 않는다는 것이다.

```hlsl
float3 AmbientLight =
    TextureColor.rgb * AmbientStrength;

float3 DiffuseLight =
    TextureColor.rgb
    * LightColor
    * Diffuse
    * DiffuseStrength
    * DistanceAttenuation;
```

Ambient는 장면 전체의 최소 밝기를 흉내 내는 값이고, 특정 Point Light에서 직접 오는 빛이 아니다. 따라서 이번 구현에서는 Point Light의 거리와 무관하게 유지한다.

---

### 6. Spot Light는 Point Light에 방향과 각도 조건을 추가한 것이다

Point Light는 위치를 중심으로 모든 방향에 빛을 보낸다.

```text
       ↑
    ↖  |  ↗
←──── Light ────→
    ↙  |  ↘
       ↓
```

Spot Light는 특정 방향을 중심으로 한 원뿔 안에만 빛을 보낸다.

```text
Light ●────────────→ SpotDirection
       \            /
        \          /
         \________/
          빛의 원뿔
```

따라서 기존 Point Light 결과에 `SpotAttenuation`을 하나 더 곱하면 된다.

```hlsl
DiffuseLight *= SpotAttenuation;
```

`SpotAttenuation`을 구하려면 다음 두 방향을 비교해야 한다.

```text
SpotDirection
→ 광원이 실제로 빛을 내보내는 중심 방향

LightToPixelDir
→ 광원에서 현재 픽셀로 향하는 방향
```

두 방향이 비슷할수록 픽셀은 원뿔 중앙에 있다.

---

### 7. Pixel→Light와 Light→Pixel의 방향을 구분해야 한다

Diffuse 계산에는 표면에서 광원으로 향하는 방향이 필요하다.

```hlsl
float3 ToLightDir =
    normalize(LightPosition - input.worldPosition);
```

```text
Pixel ─────→ Light
      ToLightDir
```

Spot Light의 각도 계산에는 반대 방향, 즉 광원에서 픽셀로 향하는 방향이 필요하다.

```hlsl
float3 LightToPixelDir = -ToLightDir;
```

```text
Light ─────→ Pixel
    LightToPixelDir
```

벡터의 부호를 바꾸면 길이는 같고 방향만 정확히 반대가 된다.

```text
v  = ( 1,  2,  3)
-v = (-1, -2, -3)
```

더 직접적으로 작성할 수도 있다.

```hlsl
float3 LightToPixelDir =
    normalize(input.worldPosition - LightPosition);
```

두 코드는 같은 의미다.

```text
-(LightPosition - WorldPosition)
= WorldPosition - LightPosition
```

---

### 8. 내적으로 원뿔 안에 있는지 판단하는 원리

정규화된 두 방향의 내적은 두 방향 사이 각도의 코사인이다.

```hlsl
float SpotCos = dot(
    normalize(SpotDirection),
    LightToPixelDir);
```

```text
SpotCos = cos(theta)
```

여기서 `theta`는 Spot Light 중심 방향과 광원에서 픽셀로 향하는 방향 사이의 각도다.

```text
theta =  0도 → SpotCos = 1.000 → 원뿔 정중앙
theta = 10도 → SpotCos ≈ 0.985
theta = 20도 → SpotCos ≈ 0.940
theta = 30도 → SpotCos ≈ 0.866
theta = 90도 → SpotCos = 0.000 → 완전히 옆쪽
```

중요한 특징은 각도가 커질수록 코사인은 작아진다는 것이다.

```text
작은 각도 → 큰 cosine
큰 각도 → 작은 cosine
```

따라서 `SpotCos`가 경계 각도의 코사인보다 크면 원뿔 안쪽이다.

```text
SpotCos >= cos(ConeAngle)
→ 원뿔 안쪽
```

CPU에서는 한 번만 각도를 코사인으로 바꾸어 Constant Buffer에 넣는다.

```cpp
LightData->SpotOuterCos =
    std::cos(DirectX::XMConvertToRadians(8.0f));

LightData->SpotInnerCos =
    std::cos(DirectX::XMConvertToRadians(4.0f));
```

Pixel Shader의 모든 픽셀에서 `cos()`를 반복 계산하지 않고 이미 계산된 경곗값과 내적 결과만 비교하기 위한 방식이다.

---

### 9. Inner Cone과 Outer Cone

경계 하나만 사용하면 빛이 갑자기 1에서 0으로 끊어진다.

```text
원뿔 안쪽 = 1
원뿔 바깥 = 0
```

손전등 가장자리를 부드럽게 만들기 위해 두 개의 경계를 둔다.

```text
Inner Cone 안쪽
→ 완전히 밝음

Inner와 Outer 사이
→ 1에서 0으로 부드럽게 감소

Outer Cone 바깥
→ 빛 없음
```

각도로 보면 Inner가 더 작다.

```text
InnerAngle = 4도
OuterAngle = 8도
```

하지만 코사인으로 바꾸면 대소 관계가 반대가 된다.

```text
cos(4도) ≈ 0.9976
cos(8도) ≈ 0.9903

SpotInnerCos > SpotOuterCos
```

이 관계를 놓치면 `smoothstep()` 인수 순서를 반대로 넣기 쉽다.

---

### 10. `step()`과 `smoothstep()`

`step(edge, x)`는 단단한 경계를 만든다.

```hlsl
float SpotAttenuation = step(SpotOuterCos, SpotCos);
```

개념적으로 다음과 같다.

```text
x < edge  → 0
x >= edge → 1
```

원뿔의 존재를 처음 검증할 때는 유용하지만 경계가 너무 날카롭다.

이번 구현은 `smoothstep()`을 사용한다.

```hlsl
float SpotAttenuation = smoothstep(
    SpotOuterCos,
    SpotInnerCos,
    SpotCos);
```

첫 번째 경계보다 작으면 0, 두 번째 경계보다 크면 1이며 그 사이를 부드럽게 연결한다.

내부적으로 생각할 수 있는 흐름은 다음과 같다.

```text
t = saturate((x - edge0) / (edge1 - edge0))
result = t²(3 - 2t)
```

단순 선형 보간 대신 `t²(3 - 2t)` 형태를 사용해 시작과 끝에서 기울기가 부드럽게 이어진다.

이번 값에서는:

```text
edge0 = SpotOuterCos = cos(8도)
edge1 = SpotInnerCos = cos(4도)
x     = SpotCos
```

이어야 한다. Outer와 Inner를 반대로 넣으면 의도한 밝기 변화가 뒤집히거나 정의하기 어려운 결과가 나온다.

---

### 11. 최종 조명식의 각 항이 하는 일

현재 직접광은 다음 식으로 계산한다.

```hlsl
float3 DiffuseLight =
    TextureColor.rgb
    * LightColor
    * Diffuse
    * DiffuseStrength
    * DistanceAttenuation
    * SpotAttenuation;
```

각 항의 역할은 다음과 같다.

```text
TextureColor
→ 물체가 원래 가진 색

LightColor
→ 조명의 색

Diffuse
→ 표면이 빛을 얼마나 정면으로 받는가

DiffuseStrength
→ 직접광 전체 세기 조절

DistanceAttenuation
→ 광원에서 멀어질수록 감소

SpotAttenuation
→ 손전등 원뿔 바깥으로 갈수록 감소
```

곱셈 중 하나라도 0이면 직접광 전체가 0이 된다.

```text
Diffuse = 0
→ 표면이 빛 반대쪽을 향함

DistanceAttenuation = 0
→ LightRange 밖에 있음

SpotAttenuation = 0
→ 손전등 원뿔 밖에 있음
```

최종 출력은 직접광과 환경광을 더한다.

```hlsl
return float4(
    AmbientLight + DiffuseLight,
    TextureColor.a);
```

---

### 12. 카메라를 손전등으로 사용하는 수학적 이유

FPS 손전등의 시작점은 카메라 위치이고 중심 방향은 카메라 Forward다.

```text
LightPosition  = Camera Position
SpotDirection = Camera Forward
```

카메라 View Matrix도 같은 Position과 Forward로 만들어진다.

```cpp
XMMatrixLookToLH(
    CameraPosition,
    GetForwardVector(),
    GetUpVector());
```

손전등에도 같은 Forward를 저장한다.

```cpp
XMStoreFloat3(
    &LightData->SpotDirection,
    MainCamera.GetForwardVector());
```

카메라 위치도 외부에서 읽을 수 있도록 다음 함수를 추가했다.

```cpp
DirectX::XMVECTOR Camera::GetCameraPosition() const
{
    return DirectX::XMLoadFloat3(&Position);
}
```

그리고 Light Position에 저장한다.

```cpp
XMStoreFloat3(
    &LightData->LightPosition,
    MainCamera.GetCameraPosition());
```

Perspective Projection에서 카메라 Forward는 화면 중앙을 통과하는 광선이다. 따라서 같은 Position과 Forward를 사용하는 Spot Light 원뿔의 중심도 항상 화면 중앙에 놓여야 한다.

```text
마우스 이동
→ Yaw/Pitch 변경
→ Camera Forward 변경
→ View Matrix 변경
→ SpotDirection도 같은 값으로 변경
→ 시야와 손전등이 함께 회전
```

최종 조명을 적용했을 때 물체에서 가장 밝아 보이는 지점은 반드시 화면 중앙일 필요가 없다. Diffuse는 표면 Normal에도 영향을 받기 때문이다. 하지만 `SpotAttenuation`만 흑백으로 출력하면 원뿔 자체의 중심은 화면 중앙과 일치해야 한다.

---

### 13. Light Constant Buffer의 확장과 16바이트 배치

손전등에 필요한 데이터를 추가하면서 Light Buffer는 다음처럼 확장됐다.

```cpp
struct alignas(16) LightBufferData
{
    DirectX::XMFLOAT3 SpotDirection;
    float AmbientStrength;

    DirectX::XMFLOAT3 LightColor;
    float DiffuseStrength;

    DirectX::XMFLOAT3 LightPosition;
    float LightRange;

    float SpotOuterCos;
    float SpotInnerCos;
    DirectX::XMFLOAT2 Padding;
};
```

HLSL도 같은 순서로 선언한다.

```hlsl
cbuffer LightBuffer : register(b2)
{
    float3 SpotDirection;
    float AmbientStrength;

    float3 LightColor;
    float DiffuseStrength;

    float3 LightPosition;
    float LightRange;

    float SpotOuterCos;
    float SpotInnerCos;
    float2 Padding;
}
```

16바이트 Register 단위로 보면 다음과 같다.

```text
0~15바이트
SpotDirection.xyz + AmbientStrength

16~31바이트
LightColor.xyz + DiffuseStrength

32~47바이트
LightPosition.xyz + LightRange

48~63바이트
SpotOuterCos + SpotInnerCos + Padding.xy
```

CPU와 HLSL의 이름 자체는 같을 필요가 없지만 자료형, 선언 순서, Offset은 일치해야 한다. 현재 크기는 64바이트이고 16의 배수다.

```cpp
static_assert(sizeof(LightBufferData) % 16 == 0);
```

`Padding`은 의미 있는 조명 데이터가 아니라 마지막 16바이트 Register를 완성하여 CPU 구조와 HLSL 배치를 명확하게 맞추기 위한 공간이다.

---

### 14. `float`와 `float3` 오타가 손전등 방향을 망가뜨린 이유

오늘 가장 오래 추적한 문제는 다음 한 줄이었다.

```hlsl
float ToObjDir = -ToLightDir;
```

`ToLightDir`은 XYZ 세 성분을 가진 `float3`인데 받는 변수를 `float` 하나로 선언했다.

```text
float3 방향 벡터
→ float 변수에 대입
→ 벡터가 Scalar 하나로 잘림
→ 완전한 3차원 방향 정보 소실
```

컴파일러는 이를 오류로 중단하지 않고 암시적 변환을 수행하며 경고만 출력했다.

```text
warning X3206: implicit truncation of vector type
```

그 결과 다음 `dot()`은 의도했던 두 `float3` 방향의 내적이 아니게 됐다.

```hlsl
float SpotCos = dot(
    normalize(SpotDirection),
    ToObjDir);
```

화면에서는 원뿔 중심이 화면 중앙이 아니라 거미의 오른쪽 다리에 나타났다. Camera와 Spot Light가 같은 Forward를 사용하고 있었기 때문에 처음에는 카메라 수학이나 행렬 문제처럼 보였지만, 실제 원인은 방향 벡터를 Scalar로 잘라버린 자료형 오타였다.

수정은 다음과 같다.

```hlsl
float3 ToObjDir = -ToLightDir;
```

또는 의미를 더 분명하게 표현한다.

```hlsl
float3 LightToPixelDir =
    normalize(input.worldPosition - LightPosition);
```

이번 문제에서 얻은 중요한 교훈은 HLSL 경고를 가볍게 보면 안 된다는 것이다. HLSL은 `float`, `float2`, `float3`, `float4` 사이의 암시적 변환을 허용하는 경우가 많다. 코드가 컴파일되더라도 좌표, 색상, Normal, 방향 성분이 조용히 사라질 수 있다.

```text
implicit truncation
→ 큰 Vector를 작은 Vector나 Scalar로 잘라냄

implicit expansion
→ Scalar를 여러 Vector 성분으로 확장할 수 있음
```

특히 위치와 방향을 다루는 코드에서 X3206 경고는 실제 화면 오류로 이어질 가능성이 매우 높으므로 오류처럼 다뤄야 한다.

---

### 15. 조명 문제를 항별로 분리해서 디버깅하는 방법

최종 조명식은 여러 값을 곱한다. 결과가 검게 나오면 한 번에 전체 식을 바라보지 말고 각 항을 화면 색으로 출력해야 한다.

#### Texture 확인

```hlsl
return DiffuseTexture.Sample(
    DiffuseSampler,
    input.texcoord);
```

Texture가 정상적으로 Bind되고 UV가 맞는지 확인한다.

#### Normal 확인

Normal 범위 `-1~1`을 색상 범위 `0~1`로 바꾼다.

```hlsl
float3 NormalColor =
    normalize(input.normal) * 0.5f + 0.5f;

return float4(NormalColor, 1.0f);
```

#### Diffuse 확인

```hlsl
return float4(Diffuse, Diffuse, Diffuse, 1.0f);
```

흰색이면 표면 Normal과 Pixel→Light 방향이 거의 같은 방향이고, 검은색이면 90도 이상 벌어져 있다.

#### Distance Attenuation 확인

```hlsl
return float4(
    DistanceAttenuation,
    DistanceAttenuation,
    DistanceAttenuation,
    1.0f);
```

광원에서 가까울수록 흰색, `LightRange`에 가까울수록 검은색으로 보여야 한다.

#### Spot Attenuation 확인

```hlsl
return float4(
    SpotAttenuation,
    SpotAttenuation,
    SpotAttenuation,
    1.0f);
```

카메라 손전등이라면 흰색 원뿔 중심이 화면 중앙과 일치해야 한다. 이번 `float`/`float3` 문제도 이 출력으로 원뿔이 오른쪽에 치우친 것을 확인하면서 찾았다.

#### `-1~1` 값을 색으로 확인

내적 결과처럼 음수를 포함한 값은 바로 색으로 출력하면 음수가 0으로 잘려 구분하기 어렵다. 다음처럼 변환한다.

```hlsl
float DebugValue = SpotCos * 0.5f + 0.5f;
return float4(DebugValue, DebugValue, DebugValue, 1.0f);
```

```text
원래 -1 → 화면 0.0 → 검정
원래  0 → 화면 0.5 → 회색
원래 +1 → 화면 1.0 → 흰색
```

이 방식은 내적, Normal 성분, 방향 성분처럼 음수를 가질 수 있는 값을 눈으로 검사할 때 반복해서 사용할 수 있다.

---

### 16. 현재 손전등 구현의 완료 범위와 한계

현재 완료된 내용은 다음과 같다.

```text
Pixel World Position 전달
Point Light 방향 계산
Point Light 거리 계산
선형 Distance Attenuation
Spot Direction 내적 판정
Inner/Outer Cone의 부드러운 경계
Camera Position과 Light Position 연결
Camera Forward와 Spot Direction 연결
Texture × Ambient × Diffuse × Distance × Spot 결합
각 조명 항을 흑백으로 분리 출력하는 디버깅
```

아직 구현하지 않은 항목은 다음과 같다.

```text
바닥과 벽에 비치는 손전등 범위
Specular Highlight
Normal Map
Shadow Map을 이용한 그림자
안개 속에서 보이는 Volumetric Light
손전등 흔들림과 밝기 변화
Monster가 원뿔 안에 있는지 확인하는 CPU 판정
게임 창이 비활성화됐을 때 마우스 고정 해제
```

현재 Spot Light는 표면에 도달한 빛만 계산한다. 공기 중에 떠 있는 빛줄기 자체는 그리지 않는다. 손전등 빛줄기가 안개 속에서 보이게 하려면 나중에 Fog 또는 Volumetric Lighting 단계가 필요하다.

또한 거미처럼 굴곡이 심한 모델에서는 Diffuse 때문에 가장 밝은 지점이 손전등 중심과 달라 보일 수 있다. 다음 단계에서 평평한 바닥이나 벽을 추가하면 원뿔의 위치, 크기, 부드러운 경계를 훨씬 명확하게 확인할 수 있다.

---

### 오늘의 핵심 요약

```text
World Position
→ 픽셀과 광원의 상대 위치를 계산하는 기준

LightPosition - WorldPosition
→ Pixel에서 Light로 향하는 벡터

length(vector)
→ 두 위치 사이의 거리

normalize(vector)
→ 길이를 1로 만들고 방향만 남김

dot(Normal, ToLightDir)
→ 표면이 빛을 얼마나 정면으로 받는지 계산

DistanceAttenuation
→ 광원에서 멀어질수록 직접광 감소

WorldPosition - LightPosition
→ Light에서 Pixel로 향하는 벡터

dot(SpotDirection, LightToPixelDir)
→ 픽셀이 손전등 중심에서 얼마나 벗어났는지 계산

Inner/Outer Cos
→ 완전히 밝은 영역과 빛이 사라지는 영역의 경계

smoothstep
→ 두 경계 사이를 부드럽게 연결

Camera Position / Forward
→ FPS 손전등의 위치와 중심 방향

float3 → float 오타
→ 방향 성분을 잃고도 컴파일될 수 있으므로 HLSL 경고를 확인해야 함
```

이번 단계의 가장 중요한 수학적 흐름은 **위치의 차이로 방향과 거리를 만들고, 정규화된 방향들의 내적으로 각도를 비교한 뒤, 거리와 각도에 따른 0~1 계수를 직접광에 곱하는 것**이다. 이 원리는 이후 Point Light, Spot Light, 시야 판정, Monster의 손전등 감지, 레이더 방향 표시에도 반복해서 사용된다.

---

## 2026-08-21: Specular, MaterialBuffer, Transform과 RenderObject

이번 단계에서는 한 개의 모델만 그리던 코드에서 벗어나 바닥, 벽, 거미를 각각 다른 위치에 배치하고, 모델의 MTL 재질에 따라 서로 다른 정반사광을 적용할 수 있도록 확장했다. 그 과정에서 Direct3D 11 파이프라인의 상태 설정 방식, Blinn-Phong 정반사 계산, CPU와 HLSL 상수 버퍼의 연결, 리소스와 인스턴스의 차이를 함께 확인했다.

현재 한 프레임의 큰 흐름은 다음과 같다.

```text
CameraBuffer 갱신
→ LightBuffer 갱신
→ 공통 렌더링 상태 설정
→ RenderObject 목록 순회
    → ObjectBuffer에 World 기록
    → Model의 Mesh 목록 순회
        → MaterialBuffer 갱신
        → Vertex/Index Buffer 바인딩
        → Texture/Sampler 바인딩
        → DrawIndexed
```

### 1. Direct3D 11은 상태를 설정한 다음 Draw한다

Direct3D 11의 Immediate Context는 현재 렌더링 상태를 기억한다. `IASetVertexBuffers`, `PSSetShaderResources`, `PSSetConstantBuffers` 같은 함수는 그 자리에서 물체를 그리는 함수가 아니다. 뒤에서 실행될 `Draw` 또는 `DrawIndexed`가 사용할 상태를 설정한다.

```cpp
DeviceContext->IASetVertexBuffers(
    0,
    1,
    &VertexBuffer,
    &Stride,
    &Offset);

DeviceContext->IASetIndexBuffer(
    IndexBuffer,
    DXGI_FORMAT_R32_UINT,
    0);

DeviceContext->PSSetShaderResources(
    0,
    1,
    &SRV);

DeviceContext->PSSetSamplers(
    0,
    1,
    &Sampler);

DeviceContext->DrawIndexed(
    IndexCount,
    0,
    0);
```

위 코드를 문장으로 읽으면 다음과 같다.

```text
IA의 0번 슬롯에 이 정점 버퍼를 연결한다.
IA가 사용할 인덱스 버퍼를 연결한다.
Pixel Shader의 t0에 이 Texture SRV를 연결한다.
Pixel Shader의 s0에 이 Sampler를 연결한다.
현재까지 설정된 모든 상태를 사용해 IndexCount만큼 그린다.
```

#### 같은 설정 함수를 여러 번 호출해도 되는 이유

같은 슬롯에 다른 리소스를 다시 설정하면 가장 마지막 설정이 현재 상태가 된다.

```cpp
Bind(SpiderMesh);
Bind(SpiderTexture);
DrawIndexed(...);

Bind(FloorMesh);
Bind(FloorTexture);
DrawIndexed(...);
```

첫 번째 `DrawIndexed`는 거미 상태를 사용하고, 두 번째 `DrawIndexed`는 바닥 상태를 사용한다. 나중에 상태를 변경해도 이미 앞에서 기록된 Draw 명령의 의미가 바뀌지는 않는다.

```text
상태 A 설정 → Draw A
상태 B 설정 → Draw B

Draw A는 상태 A 사용
Draw B는 상태 B 사용
```

따라서 Mesh가 달라지면 `IASetVertexBuffers`, `IASetIndexBuffer`를 다시 호출하는 것이 정상이고, Texture가 달라지면 `PSSetShaderResources`도 다시 호출해야 한다. 모든 Texture가 같은 Sampler를 사용한다면 Sampler는 한 번만 설정해도 되지만, 지금 단계에서는 Texture와 함께 반복해서 바인딩해도 기능상 문제는 없다.

#### 같은 슬롯과 다른 슬롯

```cpp
DeviceContext->PSSetConstantBuffers(
    2, 1, &LightBuffer);

DeviceContext->PSSetConstantBuffers(
    3, 1, &MaterialBuffer);
```

두 호출은 서로 다른 슬롯을 사용한다.

```text
Pixel Shader b2 → LightBuffer
Pixel Shader b3 → MaterialBuffer
```

같은 `b3`에 다른 버퍼를 설정하면 마지막 버퍼가 이전 버퍼를 대체한다. 같은 버퍼를 같은 슬롯에 반복해서 설정하는 것은 오류는 아니지만 중복 호출이다. 버퍼 객체는 한 번 바인딩하고, Mesh를 그리기 전에 `Map/Unmap`으로 내용만 바꿀 수도 있다.

Vertex Shader의 `b2`와 Pixel Shader의 `b2`는 서로 다른 Shader Stage의 슬롯이므로 별개의 상태다.

```text
VS b2 ≠ PS b2
```

### 2. Diffuse와 Specular의 질문은 다르다

Diffuse는 다음을 계산한다.

```text
표면이 빛을 얼마나 정면으로 받고 있는가?
```

```hlsl
float Diffuse = saturate(
    dot(Normal, ToLightDir));
```

Specular는 다음을 계산한다.

```text
표면에서 반사된 빛이 카메라로 들어오기 좋은 각도인가?
```

Diffuse는 카메라가 어디에 있는지 몰라도 계산할 수 있지만, Specular는 관찰자 방향에 따라 하이라이트 위치가 달라지므로 카메라 방향이 필요하다.

### 3. Blinn-Phong Specular에 필요한 방향

한 픽셀의 월드 위치를 `P`라고 하면 다음 방향을 만든다.

```hlsl
float3 ToLightDir = normalize(
    LightPosition - P);

float3 ToViewDir = normalize(
    CameraPosition - P);
```

```text
ToLightDir
→ 픽셀에서 광원을 바라보는 방향

ToViewDir
→ 픽셀에서 카메라를 바라보는 방향
```

현재 LightSaver에서는 손전등이 카메라에 붙어 있으므로 다음 두 위치가 같다.

```text
LightPosition == CameraPosition
```

따라서 현재 단계에서는 다음처럼 사용할 수 있다.

```hlsl
float3 ToViewDir = ToLightDir;
```

하지만 이것은 손전등과 카메라가 같은 위치에 있다는 현재 조건에서만 가능한 단순화다. 나중에 벽의 고정 조명처럼 광원과 카메라가 분리되면 `CameraPosition`을 따로 전달해야 한다.

### 4. Half Direction이 필요한 이유

빛이 카메라 쪽으로 정확하게 반사되려면 표면의 Normal이 빛 방향과 카메라 방향 사이를 적절히 나누어야 한다. Blinn-Phong 방식에서는 빛 방향 `L`과 카메라 방향 `V`의 중간 방향을 구한다.

```hlsl
float3 HalfDir = normalize(
    ToLightDir + ToViewDir);
```

단위 벡터 두 개를 더하면 두 방향 사이를 향하는 벡터가 만들어진다. 다시 정규화하여 길이를 1로 만들면 중간 방향만 남는다.

```text
L = 빛 방향
V = 카메라 방향
H = normalize(L + V)
```

`H`는 현재 빛과 카메라 배치에서 빛을 카메라로 반사하기 위해 표면이 바라봐야 하는 이상적인 Normal 방향이다.

```hlsl
float SpecularBase = saturate(
    dot(Normal, HalfDir));
```

```text
Normal == HalfDir
→ dot = 1
→ 이상적인 반사 방향
→ 가장 강한 하이라이트

Normal과 HalfDir이 직각
→ dot = 0
→ 하이라이트 없음
```

Spot Light 원뿔에서도 내적을 사용하지만 목적과 비교 대상이 다르다.

```text
Spot Light 내적
SpotDirection vs LightToPixelDirection
→ 픽셀이 손전등 원뿔 안에 있는가?

Specular 내적
Normal vs HalfDir
→ 이 표면이 빛을 카메라로 반사하기 좋은가?
```

둘 다 내적으로 각도를 비교하지만, Spot 계산은 빛의 범위를 결정하고 Specular 계산은 표면의 반짝임을 결정한다.

### 5. `pow`와 SpecularPower

내적 결과를 그대로 사용하면 방향이 대략 비슷한 넓은 영역까지 밝아져 표면 전체가 하얗게 뜬 것처럼 보일 수 있다. 이를 좁히기 위해 거듭제곱한다.

```hlsl
float Specular = pow(
    SpecularBase,
    SpecularPower);
```

`SpecularBase`는 `0~1` 범위다. `1`보다 작은 값은 거듭제곱할수록 빠르게 0으로 작아진다.

```text
pow(1.0, 32) = 1.0
pow(0.9, 32) ≈ 0.034
pow(0.8, 32) ≈ 0.0008
pow(0.5, 32) ≈ 0
```

따라서 Normal과 Half Direction이 거의 일치하는 좁은 부분만 밝게 남는다.

```text
SpecularPower가 작음
→ 넓고 흐릿한 하이라이트
→ 거친 표면처럼 보임

SpecularPower가 큼
→ 좁고 날카로운 하이라이트
→ 매끄러운 표면처럼 보임
```

`SpecularPower`가 무조건 클수록 사실적인 것은 아니다. 표면 재질에 맞는 값이 필요하다.

```text
거친 벽        → 낮은 Strength, 낮은 Power
일반 플라스틱  → 중간 Strength, 중간 Power
매끄러운 껍질  → 높은 Strength, 높은 Power
젖은 표면      → 높은 Strength, 매우 날카로운 Power
```

### 6. SpecularStrength와 SpecularPower의 차이

두 값은 역할이 다르다.

```text
SpecularStrength
→ 반짝임 전체의 밝기

SpecularPower
→ 반짝이는 영역의 넓이와 날카로움
```

최종 정반사광은 다음과 같이 계산한다.

```hlsl
float3 SpecularLight =
    LightColor
    * Specular
    * SpecularStrength
    * DistanceAttenuation
    * SpotAttenuation;
```

거리 감쇠와 원뿔 감쇠를 곱하는 이유는 이 하이라이트도 같은 손전등 빛으로 생기기 때문이다. 손전등 범위 밖이나 너무 먼 곳에서 하이라이트만 남으면 안 된다.

### 7. MTL의 `Ks`와 `Ns`

OBJ 모델의 재질 정보는 보통 MTL 파일에 들어 있다.

```mtl
Ks 0.10 0.10 0.10
Ns 16.0
```

```text
Ks
→ Specular Color
→ 반사광의 RGB 색상 또는 세기

Ns
→ Shininess
→ 하이라이트의 날카로움
```

Assimp에서는 다음 키로 읽는다.

```cpp
aiColor3D SpecularColor(
    0.2f,
    0.2f,
    0.2f);

SourceMaterial->Get(
    AI_MATKEY_COLOR_SPECULAR,
    SpecularColor);
```

현재 `MaterialData`는 Specular RGB 전체가 아니라 `float SpecularStrength` 하나만 저장한다. 그래서 RGB 중 가장 강한 성분을 대표 세기로 사용한다.

```cpp
MaterialDatas[i].SpecularStrength =
    std::max(
        SpecularColor.r,
        std::max(
            SpecularColor.g,
            SpecularColor.b));
```

예를 들어 다음 값이라면:

```text
Ks = (0.1, 0.3, 0.2)
```

```text
max(0.1, max(0.3, 0.2))
= 0.3
```

최종 `SpecularStrength`는 `0.3`이 된다. 이는 학습용 단순화다. 나중에 Material에 `float3 SpecularColor`를 그대로 저장하면 색이 있는 정반사광도 표현할 수 있다.

`Ns`는 다음처럼 읽는다.

```cpp
float SpecularPower = 32.0f;

if (SourceMaterial->Get(
        AI_MATKEY_SHININESS,
        SpecularPower) == AI_SUCCESS)
{
    MaterialDatas[i].SpecularPower =
        std::max(SpecularPower, 1.0f);
}
```

0제곱처럼 의도하지 않은 결과를 피하기 위해 최소값을 `1`로 제한했다.

### 8. Windows의 `max` 매크로와 `NOMINMAX`

Windows 헤더는 오래된 호환성을 위해 `min`, `max`라는 매크로를 만들 수 있다. 그러면 표준 라이브러리의 `std::max()`가 전처리기에서 잘못 확장된다.

```cpp
std::max(SpecularPower, 1.0f)
```

이 코드의 `max`가 Windows 매크로로 치환되면 `std::` 뒤에 올 수 없는 식이 만들어져 식별자 오류가 발생한다.

프로젝트 전처리기 정의에 다음을 추가했다.

```text
NOMINMAX
```

의미는 다음과 같다.

```text
Windows 헤더가 min/max 매크로를 정의하지 않게 한다.
```

이제 `std::min`, `std::max`를 정상적으로 사용할 수 있다.

### 9. MaterialBuffer의 CPU/HLSL 대응

재질마다 Specular 값이 다르므로 Pixel Shader에 Material 전용 상수 버퍼를 전달한다.

CPU 구조체:

```cpp
struct alignas(16) MaterialBufferData
{
    float SpecularStrength;
    float SpecularPower;
    DirectX::XMFLOAT2 Padding;
};
```

HLSL 구조체:

```hlsl
cbuffer MaterialBuffer : register(b3)
{
    float SpecularStrength;
    float SpecularPower;
    float2 MaterialPadding;
}
```

양쪽 메모리 배치는 정확히 16바이트다.

```text
SpecularStrength  4바이트
SpecularPower     4바이트
Padding           8바이트
합계             16바이트
```

상수 버퍼는 16바이트 단위 규칙에 맞춰야 하므로 다음 검사도 둔다.

```cpp
static_assert(
    sizeof(MaterialBufferData) % 16 == 0);
```

### 10. MaterialBuffer는 왜 Mesh마다 갱신하는가

Model 하나가 여러 Mesh를 가질 수 있고, 각 Mesh는 서로 다른 Material을 참조할 수 있다.

```text
Model
├── Mesh 0 → Material 0
├── Mesh 1 → Material 2
└── Mesh 2 → Material 1
```

따라서 Model을 한 번 그리는 동안에도 Material이 바뀔 수 있다. 각 Mesh의 Draw 직전에 해당 Material 값을 버퍼에 기록한다.

```text
Mesh의 MaterialIndex 확인
→ 해당 MaterialData 선택
→ MaterialBuffer Map
→ Strength/Power 기록
→ Unmap
→ Texture와 Mesh Bind
→ DrawIndexed
```

중요한 점은 MaterialBuffer 갱신이 반드시 해당 Mesh의 `DrawIndexed`보다 먼저 실행되어야 한다는 것이다.

```text
올바른 순서
Map → 기록 → Unmap → Draw

잘못된 순서
Draw → Map → 기록 → Unmap
```

잘못된 순서에서는 현재 Mesh가 이전 Mesh의 Material 또는 초기화되지 않은 값을 사용한다.

### 11. 테스트 공간: 바닥, 벽, 거미

한 개의 굴곡진 거미 모델만으로는 Spot Light의 원뿔과 Specular 범위를 판단하기 어려웠다. 평평한 표면에서 조명 결과를 확인하기 위해 바닥과 벽 OBJ를 추가했다.

```text
Assets/Models/Room/
├── Floor.obj
├── Wall.obj
├── Room.mtl
└── README.md
```

평평한 바닥과 벽은 다음을 확인하기 쉽다.

```text
손전등 원뿔이 화면 중앙에 맞는가?
Inner/Outer Cone 경계가 부드러운가?
거리 감쇠가 자연스럽게 줄어드는가?
SpecularPower에 따라 하이라이트 폭이 달라지는가?
```

### 12. Model 리소스와 Transform 인스턴스의 차이

`Model`은 파일에서 불러온 리소스다.

```text
Model
├── Mesh 목록
├── Vertex/Index Buffer
├── Texture
└── MaterialData
```

`Transform`은 그 모델을 월드의 어디에 어떤 방향과 크기로 놓을지를 나타낸다.

```text
Transform
├── Position
├── Rotation
└── Scale
```

같은 Model을 여러 곳에 배치하려면 Model 데이터를 복사하는 것이 아니라 Model 하나를 여러 Transform으로 그리면 된다.

```text
SpiderModel 한 개
├── Transform A → 복도 앞의 거미
├── Transform B → 방 안의 거미
└── Transform C → 천장의 거미
```

### 13. World Matrix의 SRT 순서

Transform의 World Matrix는 다음처럼 만든다.

```cpp
return
    XMMatrixScaling(...)
    * XMMatrixRotationRollPitchYaw(...)
    * XMMatrixTranslation(...);
```

현재 HLSL은 다음처럼 행 벡터 방식으로 곱한다.

```hlsl
float4 worldPosition =
    mul(localPosition, World);
```

따라서 World를 `S * R * T`로 구성하면 정점에는 다음 순서로 적용된다.

```text
Local Position
→ Scale
→ Rotation
→ Translation
→ World Position
```

먼저 모델 자체의 크기를 바꾸고, 그다음 모델 원점을 기준으로 회전시키고, 마지막에 월드 위치로 이동한다.

### 14. 카메라 이동과 물체 회전의 상대 운동

거미가 회전하는 동안 카메라도 옆으로 이동하면 거미가 회전하지 않는 것처럼 보일 수 있다. 이것은 Transform 오류가 아니라 상대 운동 때문이다.

화면에 나타나는 결과에는 World와 View가 함께 적용된다.

```text
World Matrix
→ 거미 자체의 위치와 회전

View Matrix
→ 카메라 위치와 방향에서 바라본 결과
```

카메라가 물체 주변을 움직이면 카메라에서 물체를 바라보는 상대 각도가 바뀐다. 그 변화가 물체 회전과 반대 방향이고 속도도 비슷하면 화면에서는 서로 상쇄될 수 있다.

현재 초기 조건을 단순하게 보면:

```text
거미 회전 속도      ≈ 1 rad/s
카메라 이동 속도     = 3 unit/s
카메라와 거미 거리   ≈ 3 unit
시점의 각속도        ≈ 속도 / 거리
                     ≈ 3 / 3
                     ≈ 1 rad/s
```

따라서 특정 방향으로 Strafe할 때 거미의 회전과 시점 변화가 거의 상쇄되어 같은 면이 계속 보이는 것처럼 느껴질 수 있다. 반대 방향으로 이동하면 두 변화가 더해져 더 빠르게 회전하는 것처럼 보일 수도 있다.

### 15. RenderObject가 필요한 이유

모델 리소스와 월드 배치 정보를 묶기 위해 `RenderObject`를 추가했다.

```cpp
struct RenderObject
{
    Model* ModelSet = nullptr;
    Transform ModelWorldTransform;
};
```

`Model*`은 Model을 복사하거나 소유하지 않고 이미 로드된 리소스를 참조한다. `Model` 내부에는 `unique_ptr<Mesh>` 같은 복사할 수 없는 소유 데이터가 있으므로 인스턴스마다 Model 전체를 복사하면 안 된다.

```text
Model*
→ 무엇을 그릴 것인가?

Transform
→ 어디에 어떻게 그릴 것인가?
```

RenderObject 목록을 순회하면 물체 수가 늘어나도 Render 함수에 개별 Draw 호출을 계속 추가할 필요가 없다.

```cpp
for (RenderObject* RenderObj : RenderObjects)
{
    if (RenderObj == nullptr)
    {
        continue;
    }

    DrawModel(
        *RenderObj->ModelSet,
        RenderObj->ModelWorldTransform
            .GetWorldMatrix());
}
```

### 16. RenderObject는 최종 Actor가 아니다

현재 RenderObject는 Model과 Transform을 묶어 여러 물체를 반복문으로 그리기 위한 중간 구조다. 최종 게임 구조에서는 게임 객체와 렌더링 제출 정보를 분리한다.

```text
World
└── Actor 목록
    ├── Transform
    └── Component 목록
        ├── MeshComponent
        ├── CollisionComponent
        ├── AudioComponent
        └── MonsterComponent
```

모든 Actor가 화면에 보이는 것은 아니므로 Actor가 Model을 직접 가지게 만들지 않는다.

```text
보이는 Actor
→ MeshComponent 보유

보이지 않는 Actor
→ Trigger, Spawn Point, Game Rule 등
→ MeshComponent가 없을 수 있음
```

앞으로 `MeshComponent`가 Model을 참조하고, Actor의 Transform과 결합하여 Renderer에 RenderObject 정보를 제공하게 된다.

### 17. 다음 구조 단계

다음 단계의 구현 순서는 다음과 같다.

```text
Actor 기본 클래스
→ Actor가 Transform 보유
→ Component 기본 클래스
→ Actor가 Component 목록 보유
→ MeshComponent가 Model 참조
→ World가 Actor 목록 보유
→ Renderer가 MeshComponent를 수집해 Draw
```

Component에는 Tick 여부를 둘 수 있다.

```text
MeshComponent
→ 매 프레임 Update할 필요 없음

MonsterComponent
→ AI 상태와 이동을 위해 Tick 필요

AudioComponent
→ 위치 또는 재생 상태에 따라 선택적으로 Tick
```

따라서 모든 객체에 무조건 Update와 Render를 넣는 구조를 피하고, 필요한 기능만 Component로 조합할 수 있다.

---

### 오늘의 핵심 요약

```text
Direct3D 11 상태 함수
→ 다음 Draw가 사용할 파이프라인 상태를 설정

같은 슬롯 재설정
→ 마지막 설정이 현재 상태가 됨

DrawIndexed
→ 호출 시점의 Mesh, Texture, Shader, Buffer 상태를 사용

Diffuse
→ 표면이 빛을 얼마나 정면으로 받는지 계산

Specular
→ 표면이 빛을 카메라로 반사하기 좋은지 계산

HalfDir
→ 빛과 카메라 사이의 이상적인 Normal 방향

pow(Base, Power)
→ 1에 가까운 값만 남겨 하이라이트 폭을 조절

MTL Ks
→ Specular 색상/세기

MTL Ns
→ Specular 날카로움

MaterialBuffer b3
→ Mesh마다 다른 재질 값을 Pixel Shader에 전달

Transform
→ Position, Rotation, Scale로 World Matrix 생성

Model
→ 정점, 인덱스, 텍스처, 재질을 가진 리소스

RenderObject
→ Model 리소스와 월드 배치를 연결한 현재 렌더링 인스턴스

Actor/Component
→ 앞으로 게임 로직과 렌더링 기능을 분리할 최종 방향
```

이번 단계의 핵심은 **Draw는 그 순간 설정된 파이프라인 상태를 사용하고, 각 Mesh의 World와 Material을 Draw 전에 갱신해야 한다는 것**이다. 이를 이해하면 이후 Renderer가 여러 Actor와 MeshComponent를 모아 그리는 구조도 같은 원리로 확장할 수 있다.

---

## 2026-08-22: Actor/Component/World와 C++ 분할 컴파일

### 1. 이번에 만든 구조

이번 단계에서는 기존의 `RenderObject`만으로 물체를 관리하던 구조에서 `World → Actor → Component` 구조로 이동하기 시작했다.

```text
LightSaverGame
└── World
    └── Actor
        ├── Transform
        └── Component 목록
            └── MeshComponent
                └── Model을 참조
```

각 클래스의 책임은 다음과 같다.

```text
World
→ 게임 안에 존재하는 Actor들을 소유하고 Update한다.

Actor
→ 하나의 게임 객체를 나타낸다.
→ 자신의 위치, 회전, 크기인 Transform을 가진다.
→ 필요한 기능을 Component로 가진다.

Component
→ Actor에 붙는 하나의 기능이다.
→ 앞으로 이동, 충돌, 소리, 몬스터 AI 같은 기능을 만들 수 있다.

MeshComponent
→ Actor를 화면에 그릴 때 사용할 Model을 참조한다.
→ Model의 소유자는 아니므로 현재는 일반 포인터로 참조한다.
```

현재 거미는 다음 흐름으로 생성된다.

```cpp
SpiderActor = GameWorld.SpawnActor<Actor>();
SpiderActor->GetActorTransform().Scale = { 0.01f, 0.01f, 0.01f };
SpiderMeshComponent = SpiderActor->AddComponent<MeshComponent>(&SpiderModel);
```

이를 문장으로 읽으면 다음과 같다.

```text
World에게 Actor 한 개를 생성해 달라고 요청한다.
→ 생성된 Actor의 크기를 정한다.
→ 그 Actor에 MeshComponent를 붙인다.
→ MeshComponent가 SpiderModel을 가리키게 한다.
```

`World`와 `Actor`는 `unique_ptr`로 내부 객체를 소유한다.

```text
World 파괴
→ World가 가진 unique_ptr<Actor> 파괴
→ Actor 자동 파괴
→ Actor가 가진 unique_ptr<Component> 파괴
→ Component 자동 파괴
```

따라서 직접 `delete`를 반복하지 않아도 소유 관계를 따라 자동으로 정리된다.

### 2. CPP 파일은 서로 내용을 직접 보지 못한다

C++ 프로젝트의 여러 CPP는 한 권의 책처럼 한꺼번에 컴파일되지 않는다. 각각 독립적으로 컴파일된다.

```text
LightSaverGame.cpp → LightSaverGame.obj
World.cpp          → World.obj
Actor.cpp          → Actor.obj
Component.cpp      → Component.obj
```

이때 `LightSaverGame.cpp`를 컴파일하는 컴파일러는 `World.cpp`의 내용을 직접 볼 수 없다.

```cpp
// LightSaverGame.cpp
#include "World.h"
```

`#include`는 헤더의 내용을 현재 CPP에 복사해서 보여주는 것에 가깝다. 따라서 컴파일러가 보는 것은 다음과 같다.

```text
World.h의 내용
+
LightSaverGame.cpp의 내용
```

`World.cpp`의 내용까지 자동으로 합쳐서 보는 것은 아니다.

### 3. 컴파일러와 링커는 하는 일이 다르다

전체 빌드는 크게 두 단계로 나눌 수 있다.

```text
1. 컴파일
CPP의 C++ 코드를 기계어가 들어 있는 OBJ로 바꾼다.

2. 링크
여러 OBJ에 이미 만들어진 함수들을 서로 연결하여 EXE를 만든다.
```

쉽게 비유하면 다음과 같다.

```text
컴파일러 = 설계도를 읽고 실제 부품을 만드는 공장
링커     = 이미 만들어진 부품들을 연결하는 조립 담당자
```

링커는 없는 C++ 함수를 새로 컴파일하지 않는다. 한 OBJ가 요구하는 완성된 함수가 다른 OBJ에 있는지 찾아 연결할 뿐이다.

### 4. 선언과 정의의 차이

선언은 함수가 존재한다는 사실과 사용 방법을 알려준다.

```cpp
void World::Update(float DeltaTime);
```

정의는 함수가 실제로 무엇을 하는지 알려준다.

```cpp
void World::Update(float DeltaTime)
{
    for (...)
    {
        Actors[i]->Update(DeltaTime);
    }
}
```

일반 함수는 헤더에서 선언만 보고 호출할 수 있다.

```text
LightSaverGame.cpp를 컴파일
→ World::Update가 존재한다는 선언을 봄
→ 실제 함수는 다른 OBJ에 있을 것이라고 기록함

World.cpp를 컴파일
→ World::Update의 본문을 봄
→ World.obj에 실제 기계어를 만듦

링크
→ LightSaverGame.obj의 요청을 World.obj의 함수와 연결
```

### 5. 불완전 타입이란 무엇인가

다음 코드는 `Actor`의 전방 선언이다.

```cpp
class Actor;
```

이 상태에서 컴파일러가 아는 내용은 하나뿐이다.

```text
Actor라는 이름의 클래스가 존재한다.
```

아직 모르는 내용은 다음과 같다.

```text
Actor의 크기
Actor의 멤버
Actor의 부모 클래스
Actor의 소멸 방법
```

이처럼 이름과 존재만 알고 전체 구조는 모르는 타입을 불완전 타입이라고 한다.

일반 포인터는 주소만 저장하므로 불완전 타입도 가리킬 수 있다.

```cpp
class Actor;
Actor* Owner = nullptr;  // 가능
```

하지만 실제 객체를 직접 멤버로 가지려면 크기를 알아야 한다.

```cpp
class Actor;
Actor Value;             // 불가능: Actor의 크기를 모름
```

### 6. 소멸자를 CPP에서 정의했던 이유

이전 구조가 다음과 같았다고 생각해 보자.

```cpp
// World.h
class Actor;

class World
{
private:
    std::vector<std::unique_ptr<Actor>> Actors;

public:
    ~World();
};
```

`unique_ptr<Actor>`는 주소를 보관하는 동안에는 불완전한 `Actor`를 사용할 수 있다. 하지만 `World`가 파괴될 때는 실제로 `Actor`를 삭제해야 한다.

```text
World 파괴
→ vector 파괴
→ unique_ptr<Actor> 파괴
→ Actor 삭제
```

따라서 `World`의 소멸 코드를 만드는 순간에는 `Actor`의 전체 정의가 필요하다.

헤더에서 다음처럼 정의하면 헤더를 포함한 CPP가 소멸자를 만들려고 할 수 있다.

```cpp
~World() = default;
```

하지만 그 위치에서 `Actor`가 전방 선언만 되어 있다면 `Actor`를 완전히 삭제하는 코드를 만들 수 없다.

그래서 헤더에는 소멸자가 존재한다는 선언만 둔다.

```cpp
// World.h
~World();
```

그리고 `Actor.h`를 볼 수 있는 CPP에서 정의한다.

```cpp
// World.cpp
#include "World.h"
#include "Actor.h"

World::~World() = default;
```

이제 `World.cpp`를 컴파일할 때 다음 정보가 한곳에 모인다.

```text
World의 전체 구조
+
Actor의 전체 구조
+
World 소멸자를 생성하라는 정의
```

`= default`는 아무 일도 하지 말라는 뜻이 아니다. 멤버를 올바른 순서로 정리하는 기본 소멸자를 컴파일러가 만들어 달라는 뜻이다.

### 7. 템플릿은 완성된 함수가 아니라 함수 제작 설명서다

다음은 함수 템플릿이다.

```cpp
template<typename ActorType>
ActorType* World::SpawnActor()
{
    auto NewActor = std::make_unique<ActorType>();
    ...
}
```

이 코드는 아직 특정한 하나의 함수가 아니다. `ActorType`에 실제 타입을 넣어 함수를 만드는 설명서다.

```cpp
SpawnActor<Actor>();
SpawnActor<MonsterActor>();
SpawnActor<PlayerActor>();
```

위의 호출들은 각각 서로 다른 실제 함수를 요구한다.

```text
SpawnActor<Actor>
SpawnActor<MonsterActor>
SpawnActor<PlayerActor>
```

컴파일러는 세상의 모든 타입을 미리 예상해서 함수를 만들 수 없다. 실제로 요청된 타입만 골라 함수를 만든다. 이 과정을 템플릿 인스턴스화라고 한다.

### 8. 템플릿 본문을 CPP에만 두면 왜 연결할 함수가 없는가

`World.cpp`에 템플릿 본문만 있다고 가정한다.

```cpp
// World.cpp
template<typename ActorType>
ActorType* World::SpawnActor()
{
    ...
}
```

`World.cpp` 안에는 `SpawnActor<Actor>()`라는 실제 호출이 없다. 그러면 이 CPP를 컴파일하는 컴파일러는 어떤 타입의 함수를 만들어야 하는지 모른다.

```text
만드는 방법은 보임
→ 템플릿 본문 있음

무엇을 만들지는 모름
→ Actor인지 MonsterActor인지 주문이 없음

결과
→ World.obj에 구체적인 SpawnActor 함수가 만들어지지 않음
```

반대로 `LightSaverGame.cpp`에는 실제 주문이 있다.

```cpp
GameWorld.SpawnActor<Actor>();
```

하지만 `World.h`에 선언만 있고 본문이 없다면 다음 상태가 된다.

```text
무엇을 만들지는 보임
→ Actor

어떻게 만들지는 모름
→ 템플릿 본문이 보이지 않음
```

결국 두 컴파일러 모두 완성된 함수를 만들지 못한다.

```text
World.cpp 쪽
→ 설명서는 있지만 주문 타입이 없음

LightSaverGame.cpp 쪽
→ 주문 타입은 있지만 설명서가 없음
```

링커가 실행되는 시점에는 이미 CPP별 컴파일이 끝났다. 링커는 한 OBJ의 주문과 다른 OBJ의 C++ 템플릿 설명서를 조합하여 새 기계어를 만들 수 없다. 따라서 실제 함수가 어느 OBJ에도 없다면 `LNK2019` 같은 링크 오류가 발생한다.

### 9. 템플릿 본문을 헤더에 두는 이유

템플릿 본문을 `World.h`에 두면 `LightSaverGame.cpp`가 헤더를 포함할 때 본문도 함께 볼 수 있다.

```text
LightSaverGame.cpp를 컴파일하는 순간

주문 타입
→ Actor

제작 설명서
→ SpawnActor 템플릿 본문

두 정보가 모두 있음
→ SpawnActor<Actor> 실제 함수 생성 가능
```

따라서 현재 `World::SpawnActor`와 `Actor::AddComponent`의 템플릿 구현은 헤더에 존재한다.

```cpp
// World.h
template<typename ActorType>
ActorType* World::SpawnActor()
{
    ...
}

// Actor.h
template<typename ComponentType, typename... Args>
ComponentType* Actor::AddComponent(Args&&... Arguments)
{
    ...
}
```

### 10. 소멸자와 템플릿이 반대로 보이는 이유

소멸자는 CPP에 정의했는데 템플릿은 헤더에 정의하므로 서로 반대 규칙처럼 보일 수 있다. 하지만 실제 규칙은 하나다.

```text
실제 기계어를 만드는 컴파일러가
그 순간 필요한 정보를 전부 볼 수 있어야 한다.
```

소멸자가 필요한 정보:

```text
Actor의 완전한 구조
```

그 정보를 확실하게 보는 위치:

```text
Actor.h를 포함한 World.cpp
```

따라서 소멸자는 CPP에서 정의했다.

템플릿이 필요한 정보:

```text
호출한 구체적인 타입
+
템플릿 함수 본문
```

구체적인 타입 주문이 나타나는 위치:

```text
LightSaverGame.cpp의 SpawnActor<Actor>() 호출
```

따라서 그 CPP가 본문도 볼 수 있도록 템플릿을 헤더에 정의한다.

### 11. 장난감 공장 비유로 다시 정리

컴파일러를 각자 닫힌 방에서 일하는 장난감 공장이라고 생각한다.

일반 소멸자는 이미 제품 이름이 정해져 있다.

```text
제품 이름: World 소멸자
필요한 설명: Actor를 없애는 방법
```

`World.cpp` 방에는 `Actor.h` 설명서가 있으므로 여기서 제품을 완성한다. 다른 방에서는 `~World()`라는 제품 이름만 보고 나중에 링커가 연결하게 한다.

템플릿은 맞춤 제작 설명서다.

```text
World.cpp 방
→ 만드는 방법은 있지만 주문서에 타입이 없음

LightSaverGame.cpp 방
→ Actor로 만들어 달라는 주문은 있지만 만드는 방법이 없음
```

서로 닫힌 방이므로 두 정보를 합칠 수 없다. 제작 설명서를 헤더에 놓으면 주문이 들어온 모든 방에서 설명서를 볼 수 있어 맞춤 제품을 만들 수 있다.

### 12. 컴파일 오류와 링크 오류의 차이

컴파일 오류는 한 CPP를 OBJ로 만드는 도중 발생한다.

```text
문법이 틀림
타입의 구조를 모름
존재하지 않는 멤버를 사용함
```

링크 오류는 각 OBJ는 만들어졌지만 마지막 연결에 실패한 것이다.

```text
함수가 있다고 선언되어 호출은 기록됨
하지만 어느 OBJ에도 그 함수의 실제 기계어가 없음
```

대표적으로 다음 메시지가 나온다.

```text
LNK2019: 확인할 수 없는 외부 기호
```

문제를 볼 때 다음처럼 구분하면 된다.

```text
컴파일 오류
→ 현재 CPP가 코드를 이해하는 데 필요한 정보가 부족한가?

링크 오류
→ 선언은 보았지만 실제 함수 정의가 OBJ 어디에도 없는가?
→ 템플릿의 구체적인 버전이 생성되지 않았는가?
```

이번에 경험한 두 문제를 빌드 단계에 맞춰 직접 비교하면 다음과 같다.

```text
1. 불완전 타입 상태에서 소멸자 코드를 만들려 한 문제

CPP 컴파일 시작
→ World를 정리하는 소멸자 코드가 필요함
→ unique_ptr<Actor>가 Actor를 삭제해야 함
→ 하지만 Actor의 전체 구조를 볼 수 없음
→ 소멸자 기계어를 만들 수 없음
→ 컴파일 오류
→ 해당 CPP의 OBJ 생성 실패
→ 링크 단계까지 진행하지 못함
```

여기서 실패하는 OBJ는 반드시 `World.obj`라는 뜻은 아니다. 헤더에 정의된 소멸자를 실제로 생성하려고 한 CPP가 실패한다. 예를 들어 `LightSaverGame.cpp`가 그 헤더를 포함하고 그 과정에서 `World`의 정리 코드를 필요로 했다면 `LightSaverGame.obj` 생성이 실패할 수 있다.

```text
2. 템플릿 본문과 구체적인 타입 주문이 서로 다른 CPP에 있던 문제

LightSaverGame.cpp 컴파일
→ SpawnActor<Actor>가 필요하다는 요청을 OBJ에 기록
→ LightSaverGame.obj 생성 가능

World.cpp 컴파일
→ 템플릿 설명서는 보이지만 어떤 타입의 주문인지 모름
→ SpawnActor<Actor> 기계어를 만들지 않음
→ World.obj 생성 가능

링크 시작
→ LightSaverGame.obj는 SpawnActor<Actor>를 요구함
→ World.obj를 포함한 어느 OBJ에도 실제 함수가 없음
→ 링크 오류 LNK2019
→ EXE 생성 실패
```

둘을 한 줄씩 줄이면 다음과 같다.

```text
불완전 타입 소멸자 문제
→ 필요한 타입 정보가 없어서 현재 CPP를 기계어로 만들지 못함
→ 컴파일 실패
→ 해당 OBJ 생성 실패

템플릿 정의 위치 문제
→ 각 CPP는 일단 OBJ가 되었지만 필요한 구체적 함수가 만들어지지 않음
→ 링크 실패
→ EXE 생성 실패
```

단, 모든 템플릿 실수가 반드시 링크 오류로 나타나는 것은 아니다. 템플릿 본문 자체에 문법이나 타입 문제가 있고 컴파일러가 그 본문을 보고 있다면 컴파일 오류가 날 수도 있다. 위 비교는 이번에 다룬 **헤더에는 템플릿 선언만 있고 CPP의 템플릿이 구체적인 타입으로 인스턴스화되지 않은 경우**를 말한다.

### 13. 현재 코드에서 다음에 고칠 확장 지점

현재 `World::SpawnActor`는 `ActorType`으로 객체를 만들지만 주소 변수는 `Actor*`로 선언되어 있다.

```cpp
auto NewActor = std::make_unique<ActorType>();
Actor* ActorAddr = NewActor.get();
```

지금은 `SpawnActor<Actor>()`만 사용하므로 동작한다. 하지만 나중에 `MonsterActor*`를 정확하게 반환하려면 다음처럼 주소 변수도 템플릿 타입으로 맞추는 것이 자연스럽다.

```cpp
ActorType* ActorAddr = NewActor.get();
```

`Actor::AddComponent`의 전달 인자는 현재 포인터를 전달하는 범위에서는 동작한다. 이후 생성자 인자가 문자열이나 이동 전용 객체까지 확장될 때 `std::forward`를 적용해 값의 전달 성질을 보존하는 단계로 발전시킬 예정이다.

### 오늘의 핵심 요약

```text
각 CPP
→ 서로 독립적으로 컴파일되어 OBJ가 됨

헤더
→ include한 CPP가 함께 볼 수 있는 정보

컴파일러
→ C++ 코드를 실제 기계어 함수로 만듦

링커
→ 이미 만들어진 함수들을 연결함
→ 템플릿을 대신 컴파일하지 않음

일반 함수
→ 헤더에 선언, CPP에 정의 가능

불완전 타입을 가진 unique_ptr 소멸
→ 실제 삭제 코드를 만드는 곳에서는 완전한 타입이 필요함

템플릿 함수
→ 구체적인 타입과 템플릿 본문을 동시에 봐야 실제 함수가 만들어짐
→ 일반적으로 본문을 헤더에 둠
```

가장 중요한 한 문장은 다음과 같다.

> 소멸자와 템플릿의 규칙이 서로 반대인 것이 아니라, 실제 코드를 만드는 컴파일러가 필요한 정보를 모두 볼 수 있는 위치에 각각의 정의를 둔 것이다.

---

## 2026-08-24: 중앙 Renderer 분리와 RenderObject 수집 파이프라인

### 오늘의 목표와 결과

이번 단계의 목표는 `LightSaverGame`이 Direct3D 11의 세부 렌더링 명령을 직접 실행하던 구조에서 벗어나, 렌더링 책임을 `Renderer` 하나로 모으는 것이었다.

변경 전 `LightSaverGame`은 다음 일을 모두 알고 있었다.

```text
게임 오브젝트 생성
카메라 입력과 이동
Shader 초기화와 Bind
Viewport 설정
Camera/Object/Light/Material Constant Buffer 생성
Constant Buffer Map/Unmap
Render Target과 Depth Buffer Clear
Actor와 MeshComponent 검색
Model Draw
COM Buffer Release
```

이 상태에서는 게임 규칙을 고치려고 `LightSaverGame`을 열어도 Direct3D 코드가 함께 보이고, 렌더링 기능을 고치려고 해도 게임 오브젝트 생성 코드와 섞여 있었다. 클래스 하나가 너무 많은 이유로 변경되는 상태였다.

이번 변경 후 책임은 다음처럼 나뉜다.

```text
LightSaverGame
├── Model 리소스 준비
├── World와 Actor 구성
├── 카메라 입력 처리
└── Renderer.Render(World, Camera) 호출

Renderer
├── Shader 소유와 Bind
├── Viewport와 Render Target 상태 설정
├── Constant Buffer 생성·갱신·해제
├── RenderObject 목록 요청
└── Model Draw 명령 제출

World / Actor / Component
└── 현재 프레임에 그릴 RenderObject 수집
```

핵심 변화는 다음 한 줄이다.

```cpp
return RenderManager.Render(GameWorld, MainCamera);
```

`LightSaverGame::Render()`는 이제 렌더링 내부 절차를 알 필요가 없다. 게임은 Renderer에게 “이 World를 이 Camera로 그려 달라”고 요청할 뿐이다.

### 1. 분리한다는 것은 단순히 코드를 다른 파일로 옮기는 것이 아니다

긴 함수를 `Renderer.cpp`로 복사했다고 해서 항상 제대로 분리된 것은 아니다. 진짜 분리는 **누가 어떤 정보를 알아야 하는가**가 달라지는 것이다.

나쁜 분리는 다음과 같다.

```text
LightSaverGame의 코드를 Renderer로 복사함
하지만 Renderer가 MonsterActor, WallActor, SpiderActor를 직접 앎
```

이 경우 파일 위치만 바뀌었을 뿐 Renderer가 여전히 게임의 구체적인 종류에 의존한다. 새로운 DoorActor를 추가할 때 Renderer까지 수정해야 한다면 책임이 충분히 분리되지 않은 것이다.

현재 목표로 한 의존 관계는 다음과 같다.

```text
게임 쪽
World → Actor → Component → RenderObject 생성

렌더링 쪽
Renderer → RenderObject를 읽어 GPU 명령 실행
```

Renderer는 그것이 거미인지, 발전기인지, 문인지 판단하지 않는다. Renderer가 필요한 질문은 다음뿐이다.

```text
어떤 Model을 그리는가?
어떤 World Transform으로 그리는가?
```

### 2. Renderer가 소유하는 것과 참조만 하는 것

현재 `Renderer`의 주요 멤버는 다음과 같다.

```cpp
Graphics* Graphic = nullptr;
ID3D11Buffer* ObjectBuffer = nullptr;
ID3D11Buffer* MaterialBuffer = nullptr;
ID3D11Buffer* LightBuffer = nullptr;
ID3D11Buffer* CameraBuffer = nullptr;
D3D11_VIEWPORT ViewPort = {};
Shader ShaderSet;
```

각 멤버의 소유 관계는 서로 다르다.

```text
Graphic
→ GameLoop 쪽에서 이미 존재하는 Graphics를 가리키는 비소유 포인터
→ Renderer가 delete하거나 Release하지 않음

ShaderSet
→ Renderer의 값 멤버
→ Renderer가 생성되고 사라질 때 함께 생성·소멸

네 개의 ID3D11Buffer*
→ Renderer가 Device를 이용해 생성한 COM 객체
→ Renderer 소멸자가 Release해야 함
```

이 차이를 모르면 이중 해제 또는 메모리 누수가 생길 수 있다.

```text
빌려 온 포인터를 해제함
→ 실제 소유자가 나중에 다시 해제하여 이중 해제 가능

직접 생성한 COM 객체를 해제하지 않음
→ 참조 카운트가 남아 리소스 누수
```

Renderer는 COM 포인터를 가지고 있으므로 복사를 금지했다.

```cpp
Renderer(const Renderer&) = delete;
Renderer& operator=(const Renderer&) = delete;
```

기본 얕은 복사를 허용하면 Renderer 두 개가 똑같은 COM 포인터를 가지게 되고, 두 소멸자가 같은 포인터에 `Release()`를 호출할 수 있기 때문이다.

### 3. Renderer 초기화 단계

`Renderer::Initialize(Graphics&)`는 한 번만 준비하면 되는 렌더링 자원을 만든다.

```text
Graphics 주소 저장
→ Viewport 값 설정
→ Shader 컴파일 및 생성
→ Camera/Object/Light/Material Constant Buffer 생성
→ Constant Buffer 슬롯 연결
```

Viewport는 백 버퍼 전체 중 래스터라이저 결과를 어느 화면 영역으로 보낼지 결정한다.

```cpp
ViewPort.TopLeftX = 0.0f;
ViewPort.TopLeftY = 0.0f;
ViewPort.Width = 1280.0f;
ViewPort.Height = 720.0f;
ViewPort.MinDepth = 0.0f;
ViewPort.MaxDepth = 1.0f;
```

현재는 창 크기를 고정값으로 사용한다. 나중에 창 크기 변경을 지원하면 실제 Client Area 크기에 맞춰 Viewport와 Projection의 Aspect Ratio를 함께 갱신해야 한다.

초기화에서 중요한 오류 처리 원칙은 다음과 같다.

```text
Shader 생성 실패
→ Renderer 초기화 실패

Constant Buffer 하나라도 생성 실패
→ Renderer 초기화 실패

Renderer 초기화 실패
→ LightSaverGame 초기화 실패
→ GameLoop 실행 시작 금지
```

반환값을 단순히 호출만 하고 무시하면 함수가 `bool`을 반환하는 의미가 사라진다. 현재 코드에서 `SetBuffers()`, `UpdateBuffers()`, `DrawWorld()`의 실패를 상위 호출자에게 전달하는 부분은 다음 정리 단계에서 마무리해야 한다.

### 4. 한 프레임의 렌더링 순서

현재 `Renderer::Render()`가 수행하는 큰 순서는 다음과 같다.

```text
1. OMSetRenderTargets
2. ClearRenderTargetView
3. ClearDepthStencilView
4. RSSetViewports
5. IASetPrimitiveTopology
6. Shader Bind
7. Camera/Light Buffer 갱신
8. World에 RenderObject 수집 요청
9. RenderObject마다 Object Buffer 갱신
10. Model Draw
```

각 함수의 역할을 한 줄씩 보면 다음과 같다.

#### `OMSetRenderTargets`

```cpp
DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
```

Output Merger 단계에 “색은 이 Render Target에 쓰고 깊이는 이 Depth Stencil에 기록하라”고 지정한다. Direct3D 11의 Device Context는 상태를 기억하므로, Draw 전에 필요한 렌더 타깃이 연결되어 있어야 한다.

#### `ClearRenderTargetView`

```cpp
DeviceContext->ClearRenderTargetView(RTV, ClearColor);
```

이전 프레임의 색을 지우고 이번 프레임의 배경색으로 채운다.

#### `ClearDepthStencilView`

```cpp
DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
```

이전 프레임의 깊이값을 지운다. 색상만 지우고 깊이를 지우지 않으면 이전 프레임의 물체가 현재 프레임 물체를 가리는 이상한 결과가 생길 수 있다.

#### `RSSetViewports`

Vertex Shader 이후 만들어진 정점의 NDC 좌표를 실제 화면의 픽셀 영역에 대응시키는 Viewport를 설정한다.

#### `IASetPrimitiveTopology`

```cpp
D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
```

Index/Vertex 데이터의 정점 세 개씩을 서로 독립적인 삼각형으로 해석하도록 Input Assembler에 알려준다.

#### `ShaderSet.Bind`

현재 그리기에 사용할 Vertex Shader, Pixel Shader, Input Layout 등을 Device Context의 파이프라인 상태로 설정한다.

중요한 원리는 다음과 같다.

```text
Direct3D 11
→ 상태 설정
→ Draw 호출
→ Draw는 그 순간 설정되어 있는 상태를 사용
```

Draw 함수가 Shader나 Buffer를 인수로 전부 받는 구조가 아닌 이유가 바로 상태 기반 API이기 때문이다.

### 5. Constant Buffer의 갱신 주기와 슬롯

현재 Constant Buffer는 역할과 갱신 주기가 다르다.

```text
CameraBuffer
→ View, Projection
→ 보통 프레임당 한 번
→ VS b0

ObjectBuffer
→ World Matrix
→ 그리는 Object마다 한 번
→ VS b1

LightBuffer
→ 손전등 위치, 방향, 색상, 범위, 원뿔 각도
→ 현재는 카메라 손전등이므로 프레임당 한 번
→ PS b2

MaterialBuffer
→ SpecularStrength, SpecularPower
→ Model 내부 Mesh/Material이 바뀔 때마다 갱신
→ PS b3
```

배열을 이용하면 연속 슬롯을 한 번에 연결할 수 있다.

```cpp
ID3D11Buffer* VsBuffers[] = { CameraBuffer, ObjectBuffer };
DeviceContext->VSSetConstantBuffers(0, 2, VsBuffers);
```

뜻은 다음과 같다.

```text
VS의 0번 슬롯 ← CameraBuffer
VS의 1번 슬롯 ← ObjectBuffer
```

Pixel Shader도 같은 방식이다.

```cpp
ID3D11Buffer* PsBuffers[] = { LightBuffer, MaterialBuffer };
DeviceContext->PSSetConstantBuffers(2, 2, PsBuffers);
```

```text
PS의 2번 슬롯 ← LightBuffer
PS의 3번 슬롯 ← MaterialBuffer
```

`Map(D3D11_MAP_WRITE_DISCARD)`는 Dynamic Buffer의 이전 내용을 버리고 CPU가 새로운 데이터를 쓸 메모리를 요청한다. `MappedResource.pData`가 실제 쓰기 주소이고, 데이터를 기록한 뒤 `Unmap()`해야 GPU가 그 내용을 사용할 수 있다.

### 6. RenderObject는 게임 오브젝트가 아니라 렌더링 요청서다

현재 `RenderObject`는 다음 두 정보를 가진다.

```cpp
struct RenderObject
{
    Model* ModelSet = nullptr;
    Transform ModelWorldTransform;
};
```

이를 말로 바꾸면 다음과 같다.

```text
ModelSet
→ 어떤 모양과 Material을 그릴 것인가

ModelWorldTransform
→ 그 Model을 이번 프레임 어디에, 어떤 회전과 크기로 그릴 것인가
```

예를 들어 거미 Actor 전체를 Renderer에 넘기는 대신 다음처럼 필요한 정보만 뽑는다.

```text
거미 Actor
├── 게임 로직과 Component 목록
├── 충돌이나 AI 상태
└── 렌더링에 필요한 정보
    ├── SpiderModel 포인터
    └── 현재 Transform 복사본
```

Renderer는 마지막 두 정보만 받는다. AI 상태나 Actor의 Component 배열은 몰라도 된다.

`Model*`은 복사하지 않고 포인터로 가리킨다. Model에는 Mesh, Texture 같은 무거운 리소스가 들어 있으므로 프레임마다 복사하면 안 된다. 이 포인터는 소유 포인터가 아니며, 실제 Model은 현재 `LightSaverGame`이 더 오래 살아 있도록 보관한다.

Transform은 작은 데이터이고 현재 프레임의 위치를 나타내므로 값으로 복사한다. 따라서 RenderObject는 다음 성격을 가진다.

```text
Model 리소스
→ 원본을 비소유 포인터로 참조

Transform
→ 이번 프레임의 상태를 값으로 저장한 스냅샷
```

이는 Unreal Engine의 Render Proxy/Scene Proxy 발상을 매우 단순화한 학습용 구조로 볼 수 있다. 실제 Unreal 구조는 게임 스레드와 렌더 스레드 분리, 더 많은 렌더 상태와 수명 동기화를 포함하므로 현재 코드와 완전히 같은 것은 아니다.

### 7. `CollectRenderObjects`의 가상 함수 흐름

Renderer가 `dynamic_cast<MeshComponent*>`로 모든 Component의 실제 타입을 검사하면 Renderer가 MeshComponent를 직접 알아야 한다.

이 의존성을 없애기 위해 Component에 가상 함수를 추가했다.

```cpp
virtual void CollectRenderObjects(
    std::vector<RenderObject>& RenderObjects) const;
```

일반 Component의 기본 구현은 아무 일도 하지 않는다.

```text
카메라 이동 Component
→ 그릴 Model이 없으므로 아무것도 추가하지 않음

AI Component
→ 그릴 Model이 없으므로 아무것도 추가하지 않음

MeshComponent
→ Model과 Owner Transform을 RenderObject로 만들어 추가
```

MeshComponent는 이 함수를 `override`한다. Actor는 Component의 정확한 자식 타입을 검사하지 않고 모든 Component에 같은 요청을 보낸다.

```cpp
Comp->CollectRenderObjects(RenderObjects);
```

여기서 `Comp`의 정적 타입이 `Component*`여도 실제 객체가 MeshComponent라면 가상 함수 호출을 통해 `MeshComponent::CollectRenderObjects()`가 실행된다.

```text
Component*가 실제로 일반 Component를 가리킴
→ Component 기본 함수 실행

Component*가 실제로 MeshComponent를 가리킴
→ MeshComponent override 함수 실행
```

이것이 런타임 다형성이며, Renderer의 `dynamic_cast`와 타입별 `if`를 없애는 핵심이다.

### 8. 하나의 vector가 Renderer부터 MeshComponent까지 이동하는 과정

`OutRenderObjects`라는 이름은 C++ 문법이 아니다. 함수가 이 매개변수에 결과를 채운다는 의도를 보여 주는 이름이다.

최초의 vector는 Renderer가 현재 프레임의 지역 변수로 만든다.

```cpp
std::vector<RenderObject> RenderObjects;
WorldSet.CollectRenderObjects(RenderObjects);
```

그 뒤 같은 vector가 참조로 전달된다.

```text
Renderer가 빈 vector 생성
          ↓ & 참조 전달
World가 모든 Actor에 전달
          ↓ 같은 vector의 참조
Actor가 모든 Component에 전달
          ↓ 같은 vector의 참조
MeshComponent가 push_back
          ↓
Renderer가 채워진 vector를 순회
```

함수 매개변수의 `&`가 중요하다.

```cpp
std::vector<RenderObject>& OutRenderObjects
```

`&`가 없다면 각 함수가 vector 복사본을 받아 자기 복사본만 수정할 수 있다. 원본에 결과가 남지 않고, 큰 목록을 여러 번 복사하는 비용도 발생한다.

`const`는 함수가 자기 객체의 상태를 바꾸지 않는다는 뜻이다.

```cpp
void Actor::CollectRenderObjects(...) const;
```

Actor의 Component를 읽고 외부에서 받은 vector에 결과를 추가하지만 Actor 자신의 Transform이나 Components 배열을 변경하지 않으므로 `const`가 성립한다.

### 9. RenderObject 목록을 매 프레임 지역 변수로 두는 이유

현재 RenderObject 목록은 Renderer의 `Render()` 또는 `DrawWorld()` 안에서 잠시 존재하는 것이 적절하다.

```text
프레임 N
→ 현재 Transform을 복사하여 RenderObject 목록 생성
→ Draw 완료
→ 목록 소멸

프레임 N+1
→ 이동·회전한 최신 Transform으로 다시 목록 생성
```

World가 목록을 영구 멤버로 보관하면 Actor가 움직일 때마다 RenderObject도 동기화해야 하고, Actor가 삭제될 때 오래된 Model 포인터를 목록에서 제거해야 한다. 지금 단계에서는 매 프레임 새 스냅샷을 만드는 편이 구조가 단순하고 안전하다.

오브젝트 수가 매우 많아져 vector의 반복 할당이 측정 가능한 문제가 되면 Renderer가 `RenderQueue`를 멤버로 보관하고 다음처럼 메모리 용량을 재사용할 수 있다.

```cpp
RenderQueue.clear();
WorldSet.CollectRenderObjects(RenderQueue);
```

`clear()`는 원소 수를 0으로 만들지만 보통 vector가 확보한 capacity는 유지하므로 다음 프레임에 같은 정도의 원소를 넣을 때 재할당을 줄일 수 있다. 하지만 지금은 성능을 추측해서 구조를 복잡하게 만들기보다 지역 변수 방식으로 기능과 책임을 먼저 검증한다.

### 10. 중첩 반복문 때문에 같은 Object를 여러 번 그렸던 문제

RenderObject 목록을 이미 모두 수집한 뒤 기존 Actor 반복문을 남겨 두면 다음 구조가 된다.

```cpp
for (Actor 3개)
{
    for (RenderObject 3개)
    {
        DrawModel();
    }
}
```

이 경우 실제 오브젝트는 세 개지만 Draw는 아홉 번 호출된다.

```text
Actor 개수 N
RenderObject 개수 N
잘못된 Draw 횟수 N × N
```

수집이 끝난 뒤 Renderer는 RenderObject 목록만 한 번 순회해야 한다.

```cpp
for (const RenderObject& RenderObj : RenderObjects)
{
    DrawModel(...);
}
```

이 문제는 화면이 얼핏 정상처럼 보여도 같은 위치에 같은 모델을 여러 번 덮어 그리기 때문에 찾기 어려울 수 있다. Draw 호출 수, RenderObject 목록 크기, 중첩 반복문의 의미를 함께 확인해야 한다.

### 11. 중앙 Renderer가 각 Actor가 스스로 Render하는 방식보다 유리한 점

각 Actor가 자신의 `Render()`에서 바로 Draw하면 처음에는 단순하다.

```text
Actor::Render
→ Mesh Bind
→ Texture Bind
→ Draw
```

하지만 렌더링 순서를 전체적으로 제어하기 어렵다.

```text
불투명 물체 먼저 출력
투명 물체를 카메라 거리순으로 정렬
같은 Shader끼리 묶어서 상태 변경 감소
그림자용으로 한 번 더 출력
레이더용 Top View Camera로 다시 출력
손전등/안개 Post Process 적용
시야 밖 Object Culling
```

이 작업들은 오브젝트 하나만 봐서는 결정하기 어렵고 전체 렌더 대상 목록을 봐야 한다. 중앙 Renderer가 RenderObject를 모으면 이후 다음 구조로 확장할 수 있다.

```text
RenderObject 수집
→ 보이는 대상만 선택
→ Shader/Material 기준 정렬
→ Main Camera Pass
→ Radar Camera Pass
→ Post Process
→ Present
```

현재 공포 게임 기획의 레이더는 같은 World를 위에서 보는 별도 Camera Pass가 될 가능성이 높다. 중앙 Renderer 구조는 이 확장에 특히 유리하다.

### 12. `Present()`가 Renderer가 아니라 GameLoop에 남는 이유

현재 한 프레임의 상위 흐름은 다음과 같다.

```text
GameLoop
├── 메시지 처리
├── Timer Tick
├── Game Update
├── LightSaverGame::Render
│   └── Renderer::Render
└── SwapChain::Present
```

Renderer는 백 버퍼에 그림을 완성한다. `Present()`는 완성된 프레임을 실제 화면에 표시하고 다음 프레임으로 넘어가는 경계를 만든다.

Present를 GameLoop에 두면 다음 원칙이 유지된다.

```text
Renderer
→ 한 프레임에 무엇을 어떻게 그릴지 담당

GameLoop
→ 한 프레임을 언제 끝내고 화면에 제출할지 담당
```

나중에 Renderer가 여러 Pass를 실행해도 모든 Pass가 끝난 뒤 Present는 프레임당 한 번만 호출하면 된다.

### 13. 현재 구조에서의 수명 관계

현재 주요 객체의 수명은 다음처럼 이해할 수 있다.

```text
LightSaverGame
├── Camera
├── Model 리소스들
├── World
│   └── unique_ptr<Actor>
│       └── unique_ptr<Component>
└── Renderer
    ├── Shader
    └── Direct3D Constant Buffer COM 객체
```

관찰용 포인터는 다음과 같다.

```text
MeshComponent::ModelSet
→ LightSaverGame이 소유한 Model을 가리킴

Component::pOwner
→ 자신을 소유한 Actor를 가리킴

RenderObject::ModelSet
→ Model 원본을 잠깐 가리키는 비소유 포인터

Renderer::Graphic
→ GameLoop이 가진 Graphics를 가리키는 비소유 포인터
```

이 포인터들은 직접 `delete`하지 않는다. 대신 실제 소유자가 포인터 사용자보다 오래 살아야 한다. 이후 ResourceManager를 만들면 Model과 Texture의 실제 소유권은 LightSaverGame에서 ResourceManager로 이동할 수 있다.

### 14. 현재 완료된 범위와 남은 정리

현재 완료된 구조:

```text
Renderer 클래스 생성
Shader/ViewPort/Clear/Constant Buffer 책임 이동
LightSaverGame의 Render 코드 축소
World → Actor → Component 수집 경로 생성
MeshComponent가 RenderObject 생성
Renderer의 dynamic_cast 제거
Renderer가 RenderObject만 순회하여 Draw
```

현재 코드에 남은 짧은 안정성 정리:

```text
Renderer::Initialize에서 SetBuffers 실패 전달
Renderer::Render에서 UpdateBuffers/DrawWorld 실패 전달
Renderer::DrawWorld에서 DrawModel 실패 전달
LightSaverGame::OnInitialize에서 Renderer 초기화 실패 전달
World와 Actor 수집 반복문에서 nullptr 방어
사용하지 않는 HRESULT와 include 제거
```

이 항목들은 구조를 바꾸는 새 기능이 아니라 실패가 조용히 무시되지 않게 만드는 마무리 작업이다.

그 뒤 렌더링의 확장 항목은 별도 단계로 취급한다.

```text
창 Resize에 따른 Viewport/Projection 재설정
ResourceManager로 Model/Texture 공유
Shader/Material별 RenderObject 정렬
Frustum Culling
투명 오브젝트 정렬
안개와 Post Process
그림자
레이더용 두 번째 Camera Pass
```

### 오늘의 핵심 요약

```text
LightSaverGame
→ 게임 상태와 입력을 담당
→ Renderer의 공개 함수만 호출

Renderer
→ Direct3D 11 상태와 GPU 리소스를 관리
→ 현재 프레임의 RenderObject 목록을 요청하여 Draw

RenderObject
→ 게임 객체 자체가 아님
→ Model 포인터와 현재 World Transform을 묶은 렌더링 요청서

CollectRenderObjects
→ Renderer가 만든 하나의 vector를 참조로 전달
→ World → Actor → Component 순서로 내려감
→ MeshComponent가 RenderObject를 push_back

virtual / override
→ Component 포인터만으로 실제 MeshComponent 구현 호출
→ dynamic_cast와 타입별 분기 제거

중앙 Renderer의 목적
→ 코드를 다른 파일로 옮기는 것에 그치지 않음
→ 게임 종류를 모르고 렌더 데이터만 처리하게 만듦
→ 정렬, Culling, 여러 Pass, 레이더 같은 확장의 기반을 만듦
```

가장 중요한 한 문장은 다음과 같다.

> World는 무엇이 존재하는지 알고, 각 Component는 자신이 어떤 렌더 데이터를 제공할지 알며, Renderer는 그 데이터가 어떤 게임 오브젝트에서 왔는지 몰라도 그릴 수 있어야 한다.
