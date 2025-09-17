struct DrawCall {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex;
    uint meshIndex;
    uint objectDataElementSize;
    uint lodCountPointerParent;
    uint firstObject;
    uint objectCount;
};

uint GetDrawCallLodCount(DrawCall drawCall) { return drawCall.lodCountPointerParent >> 28 & 0xF; }

uint GetDrawCallLodIndex(DrawCall drawCall) {
    uint id = drawCall.lodCountPointerParent >> 14 & 0x3FFF;
    if (id == 0x3FFF) return -1U;
    return id;
}
uint GetDrawCallParentIndex(DrawCall drawCall) {
    uint id = drawCall.lodCountPointerParent & 0x3FFF;
    if (id == 0x3FFF) return -1U;
    return id;
}
