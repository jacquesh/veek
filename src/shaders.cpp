// NOTE: We only have this here instead of in the usual external files because we only need to load
//       this one shader once, and not having external files means that:
//       1) We don't need to access the file system, so I guess its faster and
//       2) We don't rely on filepaths being correct, so the working directory is irrelevant, which
//          makes running it from different locations much simpler

const char* vertexShader = "\n"
    "#version 330 core\n"

    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec2 texCoord;\n"

    "uniform mat4 projectionMatrix;\n"
    "uniform mat4 viewingMatrix;\n"
    "uniform mat4 modelMatrix;\n"

    "out vec2 fragTexCoord;\n"

    "void main()\n"
    "{\n"
    "    vec4 transformedPosition = projectionMatrix*viewingMatrix*modelMatrix*vec4(position,1.0f);\n"
    "    gl_Position = transformedPosition;\n"
    "    fragTexCoord = texCoord;\n"
    "}";

const char* fragmentShader = ""
    "#version 330 core\n"

    "in vec2 fragTexCoord;\n"

    "uniform vec4 colorTint;\n"
    "uniform sampler2D spriteTex;\n"

    "out vec4 outColor;\n"

    "void main()\n"
    "{\n"
    "    vec3 textureColor = texture(spriteTex, fragTexCoord).rgb;\n"
    "    outColor = colorTint * vec4(textureColor, 1);\n"
    "}";
