
uniform sampler2D fbm_tex;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;

// Code from https://www.shadertoy.com/view/MlfSRN

#define P0  (0.5 + sin(time*0.2856) * 0.5)
#define P1  0.2
#define P2  (0.5 + sin(time*0.15) * 0.5)
#define P3  0.1
#define P4  (0.5 + sin(time * 0.2345) * 0.2)
#define P5  0.2
#define P6  0.25
#define P7  (0.5 + sin(time * 0.1945) * 0.4)


float N_i_1 (in float t, in float i)
{
    // return 1 if i < t < i+1, else return 0
    return step(i, t) * step(t,i+1.0);
}

float N_i_2 (in float t, in float i)
{
    return
        N_i_1(t, i)       * (t - i) +
        N_i_1(t, i + 1.0) * (i + 2.0 - t);
}

float N_i_3 (in float t, in float i)
{
    return
        N_i_2(t, i)       * (t - i) / 2.0 +
        N_i_2(t, i + 1.0) * (i + 3.0 - t) / 2.0;
}

float SplineValue(in float t)
{
    return
        P0 * N_i_3(t, 0.0) +
        P1 * N_i_3(t, 1.0) +
        P2 * N_i_3(t, 2.0) +
        P3 * N_i_3(t, 3.0) +
        P4 * N_i_3(t, 4.0) +
        P5 * N_i_3(t, 5.0) +
        P6 * N_i_3(t, 6.0) +
        P7 * N_i_3(t, 7.0);      
}

// F(x,y) = F(x) - y
float F ( in vec2 coords )
{
    // time in this curve goes from 0.0 to 10.0 but values
    // are only valid between 2.0 and 8.0
    float T = coords.x*6.0 + 2.0;
    return SplineValue(T) - coords.y;
}

// signed distance function for F(x,y)
float SDF( in vec2 coords )
{
    float v = F(coords);
    float slope = dFdx(v) / dFdx(coords.x);
    return abs(v)/length(vec2(slope, -1.0));
}


void main()
{
    //vec2 percent = pos;
    float dist = SDF(pos);
    /*if (dist < EDGE + SMOOTH)
    {
        dist = smoothstep(EDGE - SMOOTH,EDGE + SMOOTH,dist);
        color *= vec3(dist);
    }*/
    float colour = 1.0 - smoothstep(0.0, 0.06, dist);

    float fbm_env = max(0.0, 0.3 + 0.7 * texture(fbm_tex, pos * 7.454 + vec2(time * 0.1234, time * 0.13)).x);

    float x_env = smoothstep(0.0, 0.1, pos.x) - smoothstep(0.9, 1.0, pos.x);

    colour *= x_env * fbm_env;

    float bot_offset = texture(fbm_tex, pos * 15.0).x;
    float top_offset = texture(fbm_tex, pos * 15.0 + vec2(0, 0.5)).x;

	colour_out = vec4(colour, bot_offset, top_offset, 1.0);
}
