## tree  

This code is used to generate 64tree SDF buffer data for the GPU.  

### tldr

- TreeBuffer: Generic GPU buffer manager with staging buffer and automatic resizing
- TreeManager: 64tree builder with work-stealing thread pool
- SDF Sampling: Lipschitz-bound distance field evaluation for conservative ray marching