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
