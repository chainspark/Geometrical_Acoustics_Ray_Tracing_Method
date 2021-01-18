#define _USE_MATH_DEFINES 

#include <random>
#include <iostream>
#include <fstream>
#include <cmath>
#include <memory>

using namespace std;

static double PI = M_PI; // Marco for Pi

// -----------------Declerations ---------------------

// Object class declerations
class complex; // Complex Number 
class vec3; //3-Dimensional Vector
class ray; // Ray object
class source; //Source object
class plane; // Plane object
class box; //Shoe-box room object
class receiver; //Receiver object

//Vector function declerations
double dotProduct(vec3 vect_A, vec3 vect_B);
vec3 crossProduct(vec3 vect_A, vec3 vect_B);
double mod_vect(vec3 vect);

//Room generation function declerations
plane generate_plane(vec3 point1, vec3 point2, vec3 point3);

//Ray_tracing functions declerations
vec3 image_source_position(vec3 source, plane plane);
vec3 reflection_point(vec3 source, plane plane, ray current_ray);
double reflection_angle(plane plane, ray current_ray);
vec3 reflected_ray(plane plane, ray current_ray);
bool check_reflecting_plane(vec3 source, plane plane);
int find_reflecting_box_plane(box room, source source, int ray_no);
complex calculate_ray_pressure(source source, receiver receiver, int ray_no);
complex calculate_R(plane reflecting_plane,double angle_of_reflection);

//Reciever functions declerations
bool detect_receiver(source &source, receiver &receiver, int ray_no);
double receiver_pressure_db(receiver receiver);

// -----------------Class Definations ---------------------

class complex
{
public:
	float real;
	float imaginary;

	complex operator + (const complex& B) {
		complex sum;
		sum.real = this->real + B.real;
		sum.imaginary = this->imaginary + B.imaginary;
		return sum;
	}

	complex operator * (const complex& B) {
		complex mul;
		mul.real = (this->real) * B.real - (this->imaginary) * B.imaginary;
		mul.imaginary = (this->imaginary) * B.real + (this->real) * B.imaginary;
		return mul;
	}

	complex() = default;
	complex(const float i) :real(i), imaginary(i) {}
};

class vec3
{
public:
	float x;
	float y;
	float z;

	vec3 operator + (const vec3& vectB) {
		vec3 sum;
		sum.x = this->x + vectB.x;
		sum.y = this->y + vectB.y;
		sum.z = this->z + vectB.z;
		return sum;
	}

	vec3 operator - (const vec3& vectB) {
		vec3 diff;
		diff.x = this->x - vectB.x;
		diff.y = this->y - vectB.y;
		diff.z = this->z - vectB.z;
		return diff;
	}

	vec3 operator * (const double& B) {
		vec3 mul;
		mul.x = (this->x) * static_cast<float>(B);
		mul.y = (this->y) * static_cast<float>(B);
		mul.z = (this->z) * static_cast<float>(B);
		return mul;
	}

	vec3 operator / (const double& B) {
		vec3 div;
		div.x = (this->x) / static_cast<float>(B);
		div.y = (this->y) / static_cast<float>(B);
		div.z = (this->z) / static_cast<float>(B);
		return div;
	}

	bool operator == (const vec3 B) {
		return (this->x == B.x && this->y == B.y && this->z == B.z);
	}

	vec3() = default;
	vec3(const float i) :x(i), y(i), z(i) {}
	vec3(const float i,const float j,const float k) :x(i), y(j), z(k) {}
};

class plane
{
public:
	vec3 normal;
	vec3 point;
	double D;

	complex R_coeff;

	bool label = 0;

	plane():normal(0),point(0),D(0),R_coeff(0) {}
};

class box
{
public:
	float width, length, height;
	plane box_plane[6];
	void generate_box_room();
	
	box():width(0), length(0), height(0) {}
};



class ray
{
public:
	float theta, phi;
	vec3 direction;
	vec3 isource_position;
	static long int ray_count;
	complex R_eff;
	vec3 calc_pt;
	double pressure_square;

	ray() :theta(), phi(), direction(), isource_position(), R_eff(), calc_pt(), pressure_square()
	{
		ray_count++;
	}
	~ray(){
			ray_count--;
	};
};



class source
{
public:
	static int source_count;
	unsigned long int number_of_rays = 0;
	double power_db{}; // Source Power in Decibels
	double power{}; // Source Power in Watts
	double A{};
	double wavelength{};
	double frequency{};
	vec3 position{};
	ray* rays{}; // Pointer to array of class ray 

	void generate_source(void);
	void delete_source(void);
	void reset_rays(void);
	source(void) {
		source_count++;
		cout << "Source Created \n";
	}	
};

class receiver
{
public:
	vec3 position;
	double radius;
	unsigned long int detect_count;
	complex pressure;

	void reset(void);
};
