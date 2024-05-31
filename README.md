# MRtrix3 module for motion and distortion correction.

This module contains the SHARD reconstruction software for slice-level motion 
correction in multi-shell diffusion MRI, as described in

Daan Christiaens, Lucilio Cordero-Grande, Maximilian Pietsch, Jana Hutter, Anthony N. Price, Emer J. Hughes, Katy Vecchiato, Maria Deprez, A. David Edwards, Joseph V.Hajnal, and J-Donald Tournier, *Scattered slice SHARD reconstruction for motion correction in multi-shell diffusion MRI*, NeuroImage 117437 (2020). doi:[10.1016/j.neuroimage.2020.117437](https://doi.org/10.1016/j.neuroimage.2020.117437)


## Setup & build

```
$ git clone https://github.com/daljit/shard-recon.git
$ cd shard-recon
$ git clone https://github.com/mrtrix3/mrtrix3.git -b external_project_changes
$ cmake -B build -G Ninja
$ cmake --build build
$ cmake --install build --prefix=./installation/path #Installation path is arbitrary
$ export PATH=$PATH:./installation/path/bin
```


## Help & support

Contact daan.christiaens@kcl.ac.uk


