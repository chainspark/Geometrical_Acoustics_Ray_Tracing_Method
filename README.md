# Geometrical Acoustics: Ray Tracing Method

There are several methods that can be used for modeling of sound field. At higher frequencies, where the wavelength of sound is smaller than the objects it interacts
with, the ray-tracing method can be used to simulate the acoustic behavior of enclosed spaces when the acoustic properties of the air, surfaces and objects within
the room are known, such as the absorption coefficients of surfaces and decay law for rays propagating in air. A primary challenge in ray tracing is computation
of reverberation paths from the sound source to a receiver due to multitude of reflections and accurate simulation becomes computationally extensive. A C++
program based on object oriented programming considering the computational cost of ray tracing algorithm has been developed to simulate sound field. The algorithm
incorporates phase effects due to path length difference of sound rays to accurately model interference effects in a simple geometric virtual environment.

The sound pressure level inside a cuboidal room with only one reflecting layer (bottom layer) due to source placed at center of room is visualized below.
The Ray-tracing method is used with complex pressure variable to calculate sound pressure level.
The interference effect due to path length differences and their relation with source frequency from the reflecting plane are observed clearly.

Results for this algorthmn has been validated with the analystical solution for the following two cases
## Case I
Receiver is moved in direction perpendicular to the reflecting surface towards the origin
## Case II
Receiver is moved in direction parallel to the reflecting surface away from the the origin

### Sound source @ 250 Hz
![Sound Pressure Level @ 250 Hz](https://github.com/chainspark/Geometrical_Acoustics_Ray_Tracing_Method/blob/main/Ray-Tracing%20Method/Ray-Tracing%20Method/3d_figures/Case2_1mil_250Hz.png)
### Sound source @ 500 Hz
![Sound Pressure Level @ 500 Hz](https://github.com/chainspark/Geometrical_Acoustics_Ray_Tracing_Method/blob/main/Ray-Tracing%20Method/Ray-Tracing%20Method/3d_figures/Case2_1mil_500Hz.png)
### Sound source @ 1000 Hz
![Sound Pressure Level @ 1000 Hz](https://github.com/chainspark/Geometrical_Acoustics_Ray_Tracing_Method/blob/main/Ray-Tracing%20Method/Ray-Tracing%20Method/3d_figures/Case2_1mil_1000Hz.png)
