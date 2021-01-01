#define _USE_MATH_DEFINES // Marco for Pi

#pragma once

#include <iostream>
#include <fstream>
#include <chrono>

#include "Ray_Tracing_Algorithm.h"

using namespace std;

//Initliaze ray counter
long int ray::ray_count = 0;
int source::source_count = 0;

int main()
{
	//Generate room 
	box room;
	room.width = 4;
	room.length = 4;
	room.height = 4;

	// Generates array of planes objects according to room dimension
	room.generate_box_room();
	int no_of_planes = 6;

	//Room surface refelction coefficients
	room.box_plane[0].R_coeff.real = 0.0;//Bottom
	room.box_plane[0].R_coeff.imaginary = 0;
	room.box_plane[1].R_coeff.real = 0.0;//left
	room.box_plane[1].R_coeff.imaginary = 0;
	room.box_plane[2].R_coeff.real = 0.0;//front
	room.box_plane[2].R_coeff.imaginary = 0;
	room.box_plane[3].R_coeff.real = 0.0; //right
	room.box_plane[3].R_coeff.imaginary = 0;
	room.box_plane[4].R_coeff.real = 0.0;//back
	room.box_plane[4].R_coeff.imaginary = 0;
	room.box_plane[5].R_coeff.real = 0.0;//top
	room.box_plane[5].R_coeff.imaginary = 0;

	int reflecting_plane_no;
	
	//Generate source
	source source1;
	source1.number_of_rays = 5*1.0e6;
	source1.position.x = room.length / 2.0;
	source1.position.y = room.width / 2.0;
	source1.position.z = 3.0;

	source1.power_db = 100;
	source1.frequency = 250;//Hz

	//Termination criteria
	int reflection_order = 0;
	double threshold=0.0;

	source1.generate_source();

	//Generate reciever
	receiver receiver1;
	receiver1.position.x = source1.position.x;
	receiver1.position.y = source1.position.y;
	receiver1.position.z = 1.0;
	receiver1.radius = .025;

	//Plotting variable
	ofstream ray_tracing_plot;
	int progress = 10;
	int progress_count = 0;

	ray_tracing_plot.open("Point_.2_anechoic_250Hz_5mil_100db_cube_analytical.csv");
	
	cout << "Position of source \n";
	cout << source1.position.x << "\t" << source1.position.y << "\t" << source1.position.z << "\n";
	cout << "Source power (Watts) \t" << source1.power << "\n";
	cout << "Source frequency (Hz) \t" << source1.frequency << "\n";
	cout << "\n************************** \nRunning ray tracing algorithm\n";

	source1.generate_source();
			
	//Reset Source
	source1.reset_rays();

	//Reset Receiver
	receiver1.reset();
	
	//Timer
	auto start = std::chrono::steady_clock::now();
	
	
	for (unsigned int ray_no = 0; ray_no < source1.number_of_rays; ray_no++)
		{
			if (source1.rays[ray_no].R_eff.real == 0 && source1.rays[ray_no].R_eff.imaginary == 0) continue; // Move to next ray

			for (int r = 1; r <= reflection_order + 1; r++)
				{
					if (source1.rays[ray_no].R_eff.real == 0 && source1.rays[ray_no].R_eff.imaginary == 0)
					{
						break; // Terminate Ray
					}
					else if (detect_receiver(source1, receiver1, ray_no))
					{
						complex ray_pressure = calculate_ray_pressure(source1, receiver1, ray_no);

						double R_eff_magnitude = sqrt(source1.rays[ray_no].R_eff.real*source1.rays[ray_no].R_eff.real + source1.rays[ray_no].R_eff.imaginary*source1.rays[ray_no].R_eff.imaginary);

						if (R_eff_magnitude <= threshold) {
							break; //Terminate Ray
						}

						else {
							receiver1.pressure = receiver1.pressure + ray_pressure;
						}
					}

					// Ray Reflection

					// Find Reflection Plane
					reflecting_plane_no = find_reflecting_box_plane(room, source1, ray_no);

					// Get source image
					vec3 image = image_source_position(source1.rays[ray_no].isource_position, room.box_plane[reflecting_plane_no]);

					// Get reflection point
					vec3 rp = reflection_point(source1.rays[ray_no].isource_position, room.box_plane[reflecting_plane_no], source1.rays[ray_no]);

					// Get new direction
					vec3 reflected = reflected_ray(room.box_plane[reflecting_plane_no], source1.rays[ray_no]);

					// Get Angle
					double angle_of_reflection = reflection_angle(room.box_plane[reflecting_plane_no], source1.rays[ray_no]);

					//Calculate Reflection Coeff for the surface
					complex Rsurface = calculate_R(room.box_plane[reflecting_plane_no], angle_of_reflection);

					// Get new ray after reflection (update ray)
					source1.rays[ray_no].isource_position = image;
					source1.rays[ray_no].direction = reflected;
					source1.rays[ray_no].R_eff = source1.rays[ray_no].R_eff*Rsurface;
				}

			progress_count++;

			if (progress_count >= (progress / 100.0) * source1.number_of_rays)
			{
				cout << "Progress = " << progress << " % \n";
				progress += 10;
			}
		}


	double W_0 = source1.power;
	//Properties of air at 20 degree Celcius
	double rho = 1.2041;
	double c = 343.22;

	//Analytical Solution
	double r0 = sqrt((receiver1.position.z - source1.position.z)*(receiver1.position.z - source1.position.z));
	double r1 = sqrt((receiver1.position.z + source1.position.z)*(receiver1.position.z + source1.position.z));

	double k = (2 * PI) / source1.wavelength;

	double p2_analytical = ((W_0*rho*c) / (4 * PI))*((1 / (r0*r0)));

	double p0 = 2.0e-5;

	double analytical_pressure_db = 10 * log10(p2_analytical / (p0*p0));

	//Program Timer
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double, std::milli> elapsed_seconds = end - start;
	double total_time_milli = elapsed_seconds.count();
	double total_time = elapsed_seconds.count() / (1000.0);
	int hrs = (int)(total_time / (60.0 * 60.0));
	int minutes = (int)((total_time - hrs * 60.0 * 60.0) / 60.0);
	int seconds = (int)((total_time - hrs * 60.0 * 60.0 - minutes * 60.0));
	int millis = (int)((total_time_milli - hrs * 60.0 * 60.0*1000.0 - minutes * 60.0*1000.0 - seconds * 1000.0));
	std::cout << "elapsed time: " << hrs << "\t hrs \t" << minutes << "\t min \t" << seconds << "\t seconds \t" << millis << " milliseconds \t" << "\n";

	ray_tracing_plot << source1.number_of_rays << "," << receiver1.radius << "," << receiver_pressure_db(receiver1) << "," << analytical_pressure_db << "," << receiver1.detect_count << "," << receiver_pressure_db(receiver1) - analytical_pressure_db << "," << total_time_milli << "\n";

	source1.delete_source();

	ray_tracing_plot.close();

	cout << "Program Complete \n";
	cout << "Press Enter key to close program";

	getchar();

	return 0;
}

