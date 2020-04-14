# .vert - a vertex shader
# .tesc - a tessellation control shader
# .tese - a tessellation evaluation shader
# .geom - a geometry shader
# .frag - a fragment shader
# .comp - a compute shader

glslc texture/shader.vert -o texture/vert.spv
glslc texture/shader.frag -o texture/frag.spv

glslc notexture/shader.vert -o notexture/vert.spv
glslc notexture/shader.frag -o notexture/frag.spv

glslc color/shader.vert -o color/vert.spv
glslc color/shader.frag -o color/frag.spv

glslc ortho2d/shader.vert -o ortho2d/vert.spv
glslc ortho2d/shader.frag -o ortho2d/frag.spv

glslc text/shader.vert -o text/vert.spv
glslc text/shader.frag -o text/frag.spv
