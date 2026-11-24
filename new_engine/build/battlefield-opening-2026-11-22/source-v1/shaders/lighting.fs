#version 330

// input
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Samplers
uniform sampler2D texture0;      
uniform sampler2D skyboxTexture; 
uniform vec4 colDiffuse;

// Output
out vec4 finalColor;

#define     MAX_LIGHTS              4
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1
#define     PI 3.14159265358979323846

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);
    vec4 tint = colDiffuse * fragColor;


    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 light = vec3(0.0);

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                light = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT)
            {
                light = normalize(lights[i].position - fragPosition);
            }

            float NdotL = max(dot(normal, light), 0.0);
            lightDot += lights[i].color.rgb * NdotL;

            float specCo = 0.0;
            if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 16.0);
            specular += specCo;
        }
    }

    vec3 reflectDir = reflect(-viewD, normal);
    float phi = atan(reflectDir.z, -reflectDir.x);
    float theta = asin(reflectDir.y);
    vec2 reflectUV = vec2(phi / (2.0 * PI) + 0.5, theta / PI + 0.5);
    
    vec3 reflection = texture(skyboxTexture, reflectUV).rgb;

    vec4 baseColor = (texelColor * ((tint + vec4(specular, 1.0)) * vec4(lightDot, 1.0)));
    baseColor += texelColor * (ambient / 10.0) * tint;

    float reflectionStrength = 0.01; 
    
    //
    finalColor.rgb = baseColor.rgb + (reflection * reflectionStrength * colDiffuse.a);
    finalColor.a = baseColor.a;

    // Gamma correction
    finalColor.rgb = pow(finalColor.rgb, vec3(1.0/2.2));
}