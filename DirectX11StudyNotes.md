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
