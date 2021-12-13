///////////////////////////////////////////////////////////////////////
// A slight encapsulation of a Frame Buffer Object (i'e' Render
// Target) and its associated texture.  When the FBO is "Bound", the
// output of the graphics pipeline is captured into the texture.  When
// it is "Unbound", the texture is available for use as any normal
// texture.
////////////////////////////////////////////////////////////////////////

class FBO {
public:
    unsigned int fboID;
    unsigned int textureIDs[4];
    unsigned int depthBuffer;
    int width, height;  // Size of the texture.
    bool isBuffer = false;

    void CreateFBO(const int w, const int h, bool isBuffer);
    void Destroy();
    void Bind();
    void Unbind();
};
