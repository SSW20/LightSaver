# Room test meshes

These meshes are simple, project-owned test geometry for checking the camera-attached spot light. No external asset or license is required.

Both OBJ files use `Room.mtl`. The material intentionally has no `map_Kd`, so the current model loader should create its 1x1 white fallback texture.

The geometry is double-sided to keep the initial lighting test independent of face-culling direction.

Suggested row-vector DirectXMath world transforms:

```cpp
const DirectX::XMMATRIX FloorWorld =
    DirectX::XMMatrixScaling(12.0f, 1.0f, 12.0f)
    * DirectX::XMMatrixTranslation(0.0f, -0.45f, 4.0f);

const DirectX::XMMATRIX WallWorld =
    DirectX::XMMatrixScaling(12.0f, 4.5f, 1.0f)
    * DirectX::XMMatrixTranslation(0.0f, 1.8f, 10.0f);
```

With the current camera starting at `(0, 0, -3)`, these values make the floor extend approximately from `Z=-2` to `Z=10`, while the front wall stands at `Z=10`.
