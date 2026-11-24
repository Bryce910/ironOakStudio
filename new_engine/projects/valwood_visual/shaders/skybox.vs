#version 330

// input
in vec3 vertexPosition;

// Uniforms
uniform mat4 matProjection;
uniform mat4 matView;

//fragment shader output
out vec3 fragTexCoord;

void main()
{
    fragTexCoord = vertexPosition;
    

   
    mat4 viewNoTranslation = mat4(mat3(matView));
    

    vec4 pos = matProjection * viewNoTranslation * vec4(vertexPosition, 1.0);

    
    gl_Position = pos.xyww;
}