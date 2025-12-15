struct DrawCall {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex;
    uint meshIndex;
};

struct BatchInfo{
    uint objectDataOffset;
    uint firstObjectIndex;
    uint objectDataElementSize;
    uint drawCall;
    uint lods[4];
};
