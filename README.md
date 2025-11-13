# Aftermath Engine

Aftermath Engine is a highly customized ray marching based 3D rendering engine built with C++ and Vulkan.  
It is designed to simulate stupendously large and detailed environments using voxel data structures, while delivering truly next-generation visual effects enabled by ray-based rendering.  

## Features
- Ray Marching: Utilizes ray marching techniques for rendering complex 3D scenes.
- Ray projection onto variable aperture surface, for depth of field and bokeh effects.


## TODO/bug list

- [ ] Implement dynamic lighting and shadows.
- [ ] Implement SVO (Sparse Voxel Octree) support for efficient voxel data management.
- [ ] Add foveated rendering for performance optimization, focussing compute on the center of the screen.
- [ ] Fix destructors of uniform buffer classes to prevent memory leaks.


## Trademark & Distribution Guidelines

This project is open source under the MIT License. You're free to:
- Modify and distribute the game
- Create derivative works
- Use it commercially

However, "Eldritch Aftermath" and associated branding are protected trademarks under common law through commercial use. Under Belgian and EU law, trademark rights arise through use in commerce, and we intend to reinforce these rights through registration with the Benelux Office for Intellectual Property (BOIP).

When distributing modified versions, you must:
- Clearly indicate it's a modified version
- Use a different name (e.g., "YourGame Community Edition" or "YourGame Fork")
- Not imply it's the official version or endorsed by us
- Not use our logos, branding materials, or name in a way that causes confusion
- Optionally: Credit the original project

The official version is not yet available, but will be on Steam.

### Why This Matters
These guidelines protect players from confusion about which version they're downloading and ensure the official release maintains its reputation. We encourage community modifications and forks—just make it clear they're community versions!