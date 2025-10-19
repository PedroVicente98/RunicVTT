#shader vertex
#version 330 core

// Your existing vertex buffer layout uses location 0 for position and 1 for textCoord
// We will use position to transform the quad vertices
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textCoord; // This is not used but kept to match your VAO layout

// We will pass the world position to the fragment shader
out vec2 WorldPos;

// Uniforms for the camera matrices and the grid's model matrix
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    // The position is a vec4, so we take the xy and discard zw
    vec4 world_pos = model * vec4(position.xy, 0.0, 1.0);
    
    // Transform the world position to screen space using the camera
    gl_Position = projection * view * world_pos;
    
    // Pass the world position to the fragment shader
    WorldPos = world_pos.xy;
};


#shader fragment
#version 330 core

out vec4 color;

// We get the world position from the vertex shader
in vec2 WorldPos;

// Uniforms for grid properties
uniform int grid_type;
uniform float cell_size;
uniform vec2 grid_offset;
uniform float opacity;

void main()
{
    // Initialize color to fully transparent
    vec4 line_color = vec4(0.0, 0.0, 0.0, opacity);
    float line_thickness = 1.0;

    // Apply the user-defined offset to the world position for grid calculations
    vec2 pos = WorldPos.xy - grid_offset;
    
    if (grid_type == 0) // Square Grid
    {
        // Calculate the position relative to a cell origin
        //vec2 local_pos = mod(pos, cell_size);
        // fractional position inside the cell, robust for negatives
        vec2 local = fract(pos / cell_size) * cell_size;

        // distance to nearest vertical/horizontal line in world units
        float dx = min(local.x, cell_size - local.x);
        float dy = min(local.y, cell_size - local.y);

        // Check if the fragment is close to the left or bottom edge of a cell
        // We use an "OR" to check both horizontal and vertical lines
        //if (local_pos.x < line_thickness || local_pos.y < line_thickness)
        
        if (dx < line_thickness || dy < line_thickness)
        {
            color = line_color;
        }
        else
        {
            // If not on a line, output a fully transparent color
            color = vec4(0.0, 0.0, 0.0, 0.0);
        }
    }
  /*  else if (grid_type == 1) // Hexagonal Grid (Pointy-top)
    {
        // Hexagonal grid calculation based on axial coordinates
        float S = cell_size;
        float h = S * 0.86602540378; // h = S * sqrt(3)/2

        vec2 p = vec2(pos.x / (2.0 * h), (pos.y / (2.0 * S)) - (pos.x / (4.0 * h)));
        vec2 c = round(p);
        vec2 d = p - c;

        if (d.x > 0.0 && d.y > 0.0 && d.x + d.y > 0.5) c += vec2(1.0, 0.0);
        if (d.x < 0.0 && d.y < 0.0 && d.x + d.y < -0.5) c -= vec2(1.0, 0.0);
        if (d.x > 0.0 && d.y < 0.0 && abs(d.x) + abs(d.y) > 0.5) c += vec2(1.0, -1.0);
        if (d.x < 0.0 && d.y > 0.0 && abs(d.x) + abs(d.y) > 0.5) c -= vec2(1.0, -1.0);
        
        vec2 f = p - c;
        
        // If we are close to the center, we are not on a line
        if (dot(f, f) > 0.1) {
            float border = min(min(abs(f.x + f.y), abs(f.x - f.y)), min(abs(f.y), abs(f.x)));
            
            // Check if we are close to an edge
            if (border < line_thickness / cell_size) {
                color = line_color;
            } else {
                color = vec4(0.0, 0.0, 0.0, 0.0);
            }
        } else {
            color = vec4(0.0, 0.0, 0.0, 0.0);
        }
    }
    */
    else
    {
        // Default to transparent if an unsupported grid type is provided
        color = vec4(0.0, 0.0, 0.0, 0.0);
    }
};
