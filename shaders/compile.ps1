# .vert - a vertex shader
# .tesc - a tessellation control shader
# .tese - a tessellation evaluation shader
# .geom - a geometry shader
# .frag - a fragment shader
# .comp - a compute shader

glslc texture/shader.vert -o texture/vert.spv
glslc texture/shader.frag -o texture/frag.spv

glslc color/shader.vert -o color/vert.spv
glslc color/shader.frag -o color/frag.spv
