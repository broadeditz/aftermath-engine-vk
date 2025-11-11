# Aftermath Engine

Aftermath Engine is a ray marching based 3D rendering engine built with C++ and Vulkan.  
It is designed to simulate stupendously large and detailed environments using voxel data structures, while delivering truly next-generation visual effects enabled by ray-based rendering.  

## Features
- Ray Marching: Utilizes ray marching techniques for rendering complex 3D scenes.
- Ray projection onto variable aperture surface, for depth of field and bokeh effects.


## TODO/bug list

- [ ] Implement dynamic lighting and shadows.
- [ ] Implement SVO (Sparse Voxel Octree) support for efficient voxel data management.
- [ ] Add foveated rendering for performance optimization, focussing compute on the center of the screen.
- [ ] Fix destructors of uniform buffer classes to prevent memory leaks.