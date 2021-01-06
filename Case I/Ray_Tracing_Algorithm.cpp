#include "Ray_Tracing_Algorithm.h"


void box::generate_box_room()
{

	//generate corner Points
	vec3 c1, c2, c3, c4, c5, c6, c7, c8;
	/* ShoeBox Corners
	  c6------------c8
	 /	|		   / |
	c5------------c7 |
	|  c2---------|-c4
	| /		      | /
	c1----------- c3

	Upward : Positive Z Direction
	Right : Positive X Direction
	Forward : Positive Y Direction
	Origin at corner c1
	*/
	c1.x = 0; c1.y = 0; c1.z = 0;
	c2.x = 0; c2.y = width; c2.z = 0;
	c3.x = length; c3.y = 0; c3.z = 0;
	c4.x = length; c4.y = width; c4.z = 0;
	c5.x = 0; c5.y = 0; c5.z = height;
	c6.x = 0; c6.y = width; c6.z = height;
	c7.x = length; c7.y = 0; c7.z = height;
	c8.x = length; c8.y = width; c8.z = height;

	//Generate all planes 
	int no_of_planes = 6;

	box_plane[0] = generate_plane(c1, c3, c2);//bottom
	box_plane[1] = generate_plane(c1, c2, c5);//left
	box_plane[2] = generate_plane(c1, c5, c7);//front
	box_plane[3] = generate_plane(c3, c7, c8);//right
	box_plane[4] = generate_plane(c4, c8, c6);//back
	box_plane[5] = generate_plane(c7, c5, c6);//top

	cout << "Shoe-box room environment generated \n";
}


void source::generate_source(void)
{
	double power_ref = 1.0e-12;
	double rho = 1.2041;
	double c = 343.22;

	//Source Characteristics
	power = power_ref * pow(10, (power_db / 10.0));

	A = sqrt((rho * c * (power / number_of_rays)) / (4 * PI));

	wavelength = c / frequency;

	rays = new ray[number_of_rays];

	unsigned int ray_no;

	//Generate a random ray directions

	//Random Number Generator
	random_device seed; // Seed to make randon number underministic
	mt19937 generator(seed()); // Algorithm for random number
	uniform_real_distribution<float> uniform_random(0.0, 1.0); //Distribution Type and range

	//Initialize Rays 
	for (ray_no = 0; ray_no < number_of_rays; ray_no++) {

		//Ray source Position
		rays[ray_no].isource_position = this->position;

		//Ray Initial Reflection Coefficient
		rays[ray_no].R_eff.real = 1.0;
		rays[ray_no].R_eff.imaginary = 0.0;

		// Generate Ray direction vectors
		rays[ray_no].theta = 2 * (float)PI * uniform_random(generator);
		rays[ray_no].phi = (float)acos(2 * uniform_random(generator) - 1.0);

		rays[ray_no].direction.x = cos(rays[ray_no].theta) * sin(rays[ray_no].phi);
		rays[ray_no].direction.y = sin(rays[ray_no].theta) * sin(rays[ray_no].phi);
		rays[ray_no].direction.z = cos(rays[ray_no].phi);

	}

	cout << "Rays vectors generated \n";
}

void source::reset_rays(void) {
	unsigned int ray_no;

	for (ray_no = 0; ray_no < number_of_rays; ray_no++) {
		rays[ray_no].isource_position = this->position;
		rays[ray_no].R_eff.real = 1;
		rays[ray_no].R_eff.imaginary = 0;

		rays[ray_no].direction.x = cos(rays[ray_no].theta) * sin(rays[ray_no].phi);
		rays[ray_no].direction.y = sin(rays[ray_no].theta) * sin(rays[ray_no].phi);
		rays[ray_no].direction.z = cos(rays[ray_no].phi);
	}
}

void source::delete_source(void) {
	delete[] rays;
	source_count--;
}


void receiver::reset(void)
{
	pressure.real = 0;
	pressure.imaginary = 0;
	detect_count = 0;
}


// -----------------Function Definations --------------------- 

double dotProduct(vec3 vect_A, vec3 vect_B)
{
	double dot_product = static_cast<double>(vect_A.x * vect_B.x + vect_A.y * vect_B.y + vect_A.z * vect_B.z);
	return dot_product;
}

vec3 crossProduct(vec3 vect_A, vec3 vect_B)
{
	vec3 cross_product;

	cross_product.x = vect_A.y * vect_B.z - vect_A.z * vect_B.y;
	cross_product.y = vect_A.z * vect_B.x - vect_A.x * vect_B.z;
	cross_product.z = vect_A.x * vect_B.y - vect_A.y * vect_B.x;

	return cross_product;
}

double mod_vect(vec3 vect)
{
	double mod;
	mod = sqrt(pow(vect.x, 2) + pow(vect.y, 2) + pow(vect.z, 2));
	return mod;
}

plane generate_plane(vec3 point1, vec3 point2, vec3 point3)
{
	plane plane;

	double mod_N;
	double D;

	vec3 v1, v2;
	vec3 N, n;

	v1 = point3 - point2;

	v2 = point1 - point2;

	N = crossProduct(v1, v2);

	mod_N = mod_vect(N);

	n = N / mod_N;

	D = -1.0 * dotProduct(n, point1);

	plane.normal.x = n.x;
	plane.normal.y = n.y;
	plane.normal.z = n.z;
	plane.D = D;

	return plane;
}

vec3 image_source_position(vec3 source_position, plane plane)
{
	vec3 image_source_position;

	double dist_along_normal = dotProduct(plane.normal, source_position) + plane.D;

	image_source_position = source_position - (plane.normal * (2 * dist_along_normal));

	return image_source_position;
}

vec3 reflection_point(vec3 source_position, plane plane, ray current_ray)
{
	vec3 reflection_point;

	double dist_along_normal = dotProduct(plane.normal, source_position) + plane.D;

	double dist_along_ray = (-1.0 * dist_along_normal) / dotProduct(current_ray.direction, plane.normal);

	reflection_point = source_position + (current_ray.direction * dist_along_ray);

	return reflection_point;
}

double reflection_angle(plane plane, ray current_ray)
{
	double theta;

	theta = acos(-1.0 * dotProduct(plane.normal, current_ray.direction));

	return theta;// In Radians
}

vec3 reflected_ray(plane plane, ray current_ray)
{
	vec3 reflected_ray;

	reflected_ray = current_ray.direction - (plane.normal * (2.0 * dotProduct(plane.normal, current_ray.direction)));

	return reflected_ray;
}

bool check_reflecting_plane(vec3 source_position, plane plane)
{
	bool reflecting = 0;

	double dist_along_normal = dotProduct(plane.normal, source_position) + plane.D;

	if (dist_along_normal <= 0)
	{
		reflecting = false;
	}
	else if (dist_along_normal > 0)
	{
		reflecting = true;
	}

	return reflecting;
}

int find_reflecting_box_plane(box room, source source, int ray_no)
{
	int no_of_planes = 6;
	int reflecting_plane_no = -1;

	// Reset plane labels
	for (int j = 0; j < no_of_planes; j++)
	{
		room.box_plane[j].label = 0;
	}

	double min_dist = 1.0e8;//initialise with bad value 

	for (int j = 0; j < no_of_planes; j++)
	{
		bool reflecting = check_reflecting_plane(source.rays[ray_no].isource_position, room.box_plane[j]);

		if (reflecting)
		{
			room.box_plane[j].label = 1;
		}

		if (room.box_plane[j].label)
		{
			vec3 normal, isource_position, ray_direction;
			normal = room.box_plane[j].normal;
			isource_position = source.rays[ray_no].isource_position;
			ray_direction = source.rays[ray_no].direction;

			// Get distance of plane along ray direction
			double dist_along_normal = dotProduct(normal, isource_position) + room.box_plane[j].D;
			double dist_along_ray = (-1.0 * dist_along_normal) / dotProduct(ray_direction, normal);

			if (dist_along_ray <= min_dist && dist_along_ray > 0)
			{
				// Get plane with minimum distance and retrieve plane number for that plane
				min_dist = dist_along_ray;
				reflecting_plane_no = j;
			}
		}
	}

	if (reflecting_plane_no == -1) cout << "Error : Can't find reflecting plane \n Source and receiver need to be inside the room \n";

	return reflecting_plane_no;
}


complex calculate_ray_pressure(source source, receiver receiver, int ray_no)
{
	double path_length = mod_vect(receiver.position - source.rays[ray_no].isource_position);

	double D_ray = 2 * sqrt(pow(receiver.radius, 2) - pow(mod_vect(source.rays[ray_no].calc_pt - receiver.position), 2));

	double V = (4 / 3) * PI * pow(receiver.radius, 3);

	double L = sqrt(PI * pow(receiver.radius, 2));

	double rho = 1.2041;
	double c = 343.22;

	double Po = 2 * path_length * ((sqrt(rho * c * source.power * PI * D_ray / V)) / (source.number_of_rays * L));

	complex pressure_ray;
	//Complex Pressure Function
	pressure_ray.real = (float)(cos((2 * PI * (path_length)) / source.wavelength) * (Po));
	pressure_ray.imaginary = (float)(sin((2 * PI * (path_length)) / source.wavelength) * (Po));

	//Multiplication by refflection coefficient
	pressure_ray = source.rays[ray_no].R_eff * pressure_ray;

	source.rays[ray_no].pressure_square = (pressure_ray.real * pressure_ray.real) + (pressure_ray.imaginary * pressure_ray.imaginary);

	return pressure_ray;
}

double receiver_pressure_db(receiver receiver)
{
	//Convert to Db
	double p0 = 2.0e-5;
	double pressure_db = 20 * log10(sqrt((receiver.pressure.real * receiver.pressure.real) + (receiver.pressure.imaginary * receiver.pressure.imaginary)) / p0);
	return pressure_db;
}

complex calculate_R(plane reflecting_plane, double angle_of_reflection)
{
	complex R;
	R = reflecting_plane.R_coeff;
	return R;
}

bool detect_receiver(source& source, receiver& receiver, int ray_no)
{
	vec3 ray_source = source.rays[ray_no].isource_position;
	vec3 ray_direc = source.rays[ray_no].direction;
	vec3 receiver_pos = receiver.position;
	double receiver_radius = receiver.radius;

	// Find the perpendicular distance of ray from reciever center 
	double perp_dist;
	vec3 distance = receiver_pos - ray_source;

	double dist_along_ray = dotProduct(distance, ray_direc);

	if (dist_along_ray < 0) return false;

	// Find point along direction of ray with dotproduct magnitude 
	vec3 test_pt = ray_source + ray_direc * dist_along_ray;

	// Evaluate the distance of that point with reciever position
	perp_dist = sqrt(pow((test_pt.x - receiver_pos.x), 2) + pow((test_pt.y - receiver_pos.y), 2) + pow((test_pt.z - receiver_pos.z), 2));

	if (perp_dist < receiver_radius && perp_dist >= 0) {
		//Update Test Point for ray
		source.rays[ray_no].calc_pt = test_pt;
		receiver.detect_count++;
		return true;
	}
	else {
		//Clear calc_pt as ray is not detected reciever
		source.rays[ray_no].calc_pt.x = 0;
		source.rays[ray_no].calc_pt.y = 0;
		source.rays[ray_no].calc_pt.z = 0;
		return false;
	}
}
