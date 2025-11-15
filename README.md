# Aftermath Engine

Aftermath Engine is a highly custom ray marching based 3D rendering engine built with C++ and Vulkan.  
It is designed to simulate stupendously large and detailed destructable environments using voxel SDF data structures, while delivering truly next-generation visual effects enabled by ray-based rendering.  

### Please note:
This project is still in early development. Many features are incomplete or missing, and there may be bugs and performance issues. Try at your own risk!  
If you're new to game development, and wondering if this is a good project to learn from, the answer is definitely no. The codebase is not well documented yet.  
Especially the `main.cpp` is messy, because I use it like a sketchpad, before extracting functionality into their own files.  
However, if you're experienced with C++ and graphics programming, feel free to explore and contribute!  

## Features
- Ray Marching: Utilizes ray marching techniques for rendering complex 3D scenes.
- Ray projection onto variable aperture surface, for depth of field and bokeh effects.


## TODO/bug list

- [ ] Implement dynamic lighting and shadows.
- [x] Implement sparse signed distance field 64tree support for efficient voxel data management. (Early, naive, implementation done)
	- [ ] Implement dynamic voxel destruction
	- [ ] Implement dynamic voxel construction
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
- Use a different name (e.g., "Eldritch Aftermath Community Edition" or "Eldritch Aftermath Fork")
- Not imply it's the official version or endorsed by us
- Not use our logos, branding materials, or name in a way that causes confusion
- Optionally: Credit the original project

The official version is not yet available, but will be on Steam.

### Why This Matters
These guidelines protect players from confusion about which version they're downloading and ensure the official release maintains its reputation. We encourage community modifications and forks—just make it clear they're community versions!