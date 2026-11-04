struct BoltVertex
{
    float4 position : POSITION;
    float2 acrossAndBrightness : TEXCOORD0;
};

BoltVertex main(BoltVertex input)
{
    return input;
}
