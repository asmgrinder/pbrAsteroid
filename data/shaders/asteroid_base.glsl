
#define height_mapping(x) (0.95 + 0.15 * x)

#define rads(x) (x * 3.1415926535897932384626433832795 / 180)

#define START_LEVEL 1
#define MAX_LEVEL 22

#define ICO_POINT0_X 0.8944271909999159
#define ICO_POINT0_Y 0.
#define ICO_POINT0_Z 0.447213595499958

#define ICO_POINT1_X 0.7236067977499789
#define ICO_POINT1_Y 0.5257311121191336
#define ICO_POINT1_Z -0.447213595499958

#define PHI 1.61803398874989484820459

float random3D(in vec3 p)
{
	vec2 arg = p.xy * (1. + log(p.z + 10.5));
	return fract(sin(dot(arg * rads(180) / 4096., vec2(12.9898, 78.233))) * 43758.5453123);
}

float random2D (in vec2 p)
{
	return random3D(vec3(p, PHI));
}

vec2 rotate2D(vec2 src, float ang)
{
	float sinang = sin(ang);
	float cosang = cos(ang);
	return vec2(src.x * cosang - src.y * sinang,
				src.x * sinang + src.y * cosang);
}

float g(float ro, float v, float radius)
{
	float _ro = ro / radius;
	return -(cos((rads(180) - v) * _ro * _ro + v) + 1.0);
}

vec2 gdiff(vec2 ro, float v, float radius)
{
	float _ro = length(ro) / radius;
	float pi_v = rads(180) - v;
	return 2.0 * pi_v * _ro * sin(pi_v * _ro * _ro + v) * normalize(ro);
}

float gdiff2(float ro, float v, float radius)
{
	float _ro = ro / radius;
	float pi_v = rads(180.0) - v;
	return 2.0 * pi_v * _ro * sin(pi_v * _ro * _ro + v);
}

// recognize which icosaedr triangle the point is in and rotate point to convenient one
void recognizeIcoTriangle(in vec3 Pos, out vec3 localPos, out vec3 localNormal, out float rotZang, out float rotYang, out vec2 rotYdisp)
{
	const float ang36 = rads(36);
	const float ang72 = rads(72);

	float angxy = atan(Pos.y, Pos.x);
	rotZang = -floor((angxy + ang36) / ang72) * ang72;      // remember transform to be able to make inverse one
	vec3 local_pos = vec3(rotate2D(Pos.xy, rotZang), Pos.z);
	vec3 local_normal = local_pos;
	float divider = (ICO_POINT1_X - ICO_POINT0_X) * local_pos.z + (ICO_POINT0_Z - ICO_POINT1_Z) * local_pos.x;
	if (divider > 0.)
	{   // project to lower central belt plane
		local_pos *= (ICO_POINT0_Z * ICO_POINT1_X - ICO_POINT0_X * ICO_POINT1_Z) / divider;
	}
	rotYang = 0.;                                           // remember transform to be able to make inverse one
	rotYdisp = vec2(0., 0.);                                // remember transform to be able to make inverse one
	if (divider > 0.
		&& local_pos.z <= ICO_POINT1_Z)
	{   // lower belt
		local_pos *= ICO_POINT1_X / ((1 + ICO_POINT1_Z) * local_pos.x - ICO_POINT1_X * local_pos.z);
	}
	else if (divider > 0.
				&& local_pos.z <= ICO_POINT0_Z
				&& abs(local_pos.y) <= ICO_POINT1_Y * (local_pos.z - ICO_POINT0_Z) / (ICO_POINT1_Z - ICO_POINT0_Z))
	{   // lower central belt
		rotYang = rads(138.1896851042214);                  // remember transform to be able to make inverse one
		rotYdisp = vec2(ICO_POINT1_X, ICO_POINT1_Z);        // remember transform to be able to make inverse one
		local_pos.xz = rotate2D(local_pos.xz - rotYdisp, rotYang) + rotYdisp;
		local_normal.xz = rotate2D(local_normal.xz, rotYang);
	}
	else
	{
		rotZang = -(floor(angxy / ang72) * ang72 + ang36);  // remember transform to be able to make inverse one
		local_pos = vec3(rotate2D(Pos.xy, rotZang), Pos.z);
		local_normal = local_pos;
		divider = (ICO_POINT1_X - ICO_POINT0_X) * local_pos.z + (ICO_POINT1_Z - ICO_POINT0_Z) * local_pos.x;
		if (divider < 0)
		{
			local_pos *= (ICO_POINT0_X * ICO_POINT1_Z - ICO_POINT0_Z * ICO_POINT1_X) / divider;
			if (local_pos.z >= ICO_POINT0_Z)
			{   // upper belt
				local_pos *= ICO_POINT1_X / ((1 - ICO_POINT0_Z) * local_pos.x + ICO_POINT1_X * local_pos.z);
			}
			else
			{   // upper central belt
				rotYang = rads(-138.1896851042214);         // remember transform to be able to make inverse one
				rotYdisp = vec2(ICO_POINT1_X, ICO_POINT0_Z);// remember transform to be able to make inverse one
				local_pos.xz = rotate2D(local_pos.xz - rotYdisp, rotYang) + rotYdisp;
				local_normal.xz = rotate2D(local_normal.xz, rotYang);
			}
		}
	}
	localPos = local_pos;
	localNormal = local_normal;
}

int getIntPow(in int Pow)
{   // get power of 2 for start level
	int intPow = 1;
	for (int i = 0; i < Pow; i++)
	{
		intPow *= 2;
	}
	return intPow;
}

vec2 localPosToGrid(in vec3 localpos, in int intPow, out vec2 cr1, out vec2 cr2, out vec2 cr3)
{   // recognize which triangle in the triangle grid current point belongs to
	// calculate triangle vertex coordinates and current point displacement inside it
	vec2 frac = vec2(0., 0.);
	float xpow2 = intPow * (1. - localpos.x / ICO_POINT1_X);
	float xint = trunc(xpow2);
	frac.x = clamp(xpow2 - xint, 0., 1.);

	float ypow2 = (intPow * (localpos.y + ICO_POINT1_Y) - ICO_POINT1_Y * xint) * .5 / ICO_POINT1_Y;
	float yint = floor(ypow2);
	frac.y = ypow2 - yint;

	cr1 = vec2(xint, yint);
	cr2 = vec2(xint, yint + 1);
	cr3 = vec2(xint + 1, yint);
	if (frac.x > frac.y * 2.                    // check if grid triangle is upside down
		|| frac.x > (1. - frac.y) * 2.)
	{
		yint   += (frac.y < .5) ? -1. : 0.;
		frac.y += (frac.y < .5) ? .5 : -.5;
		frac.x = 1. - frac.x;

		cr1 = vec2(xint + 1., yint);
		cr2 = vec2(xint + 1., yint + 1.);
		cr3 = vec2(xint, yint + 1.);
	}

	return frac;
}

// rotate and displace triangle in the triangle grid vertex to get coordinates in source coordinate system
void gridToGlobal(in vec2 cr, in vec3 localpos, in int intPow, in vec2 rotYdisp, in float rotYang, in float rotZang,
					out vec3 pos, out vec3 nmldisp)
{
	float zSign = sign(localpos.z);
	zSign = (0. == zSign) ? 1. : zSign;
	vec2 _pos = vec2(ICO_POINT1_X * (1. - cr.x / intPow),
						ICO_POINT1_Y * (2. * cr.y + cr.x - intPow) / intPow);
	pos = vec3(_pos, (1. - _pos.x * (1. - ICO_POINT0_Z) / ICO_POINT1_X) * zSign);
	nmldisp = normalize(localpos - pos);
	pos.xz = rotate2D(pos.xz - rotYdisp, -rotYang) + rotYdisp;
	pos.xy = rotate2D(pos.xy, - rotZang);

	pos += 1.2 * vec3(1., 1., 1.);

	pos = trunc(pos * intPow * 1.5);
}

// to get height for point in the triangle code uses interpolation
// based on line from triangle vertex to opposite side
// to avoid visual artefacts it does this for every triangle vertex
// and averages the result
// code uses smoothstep to avoid visibility of edges of triangles in the grid
float calculateHeightAndNormal(in float rnd1, in float rnd2, in float rnd3,
								in int intPow, in vec2 frac, in float outside_crater,
								in vec3 nmldisp1, in vec3 nmldisp2, in vec3 nmldisp3,
								out vec3 normal)
{
	// intersection points of triangle side and line on opposite vertex and current point
	float rnd23 = 0.;
	float t1 = 0.;
	float divider1 = 2. * frac.y + frac.x;
	if (0 != divider1)
	{
		vec2 ip1 = vec2(2. * frac.x, 2. * frac.y) / divider1;
		rnd23 = smoothstep(0., 1., 1. - ip1.x) * (rnd2 - rnd3) + rnd3;
		float lip1 = length(ip1);
		lip1 = (0. == lip1) ? 1. : lip1;
		t1 = clamp(length(frac) / lip1, 0., 1.);
	}
	float rnd13 = 0.;
	float t2 = 0.;
	float divider2 = frac.x - 2. * frac.y + 2.;
	if (0 != divider2)
	{
		vec2 ip2 = vec2(2. * frac.x, frac.x) / divider2;
		rnd13 = smoothstep(0., 1., 1. - ip2.x) * (rnd1 - rnd3) + rnd3;
		float lip2 = length(ip2 - vec2(0., 1.));
		lip2 = (0. == lip2) ? 1. : lip2;
		t2 = clamp(length(frac - vec2(0., 1.)) / lip2, 0., 1.);
	}
	float rnd12 = 0.;
	float t3 = 0.;
	float divider3 = 2. * frac.x - 2.;
	if (0 != divider3)
	{
		vec2 ip3 = vec2(0., frac.x - 2. * frac.y) / divider3;
		rnd12 = smoothstep(0., 1., 1. - ip3.y) * (rnd1 - rnd2) + rnd2;
		float lip3 = length(ip3 - vec2(1., 0.5));
		lip3 = (0. == lip3) ? 1. : lip3;
		t3 = clamp(length(frac - vec2(1., 0.5)) / lip3, 0., 1.);
	}

	float h1 = smoothstep(0., 1., 1. - t1) * (rnd1 - rnd23) + rnd23;
	float h2 = smoothstep(0., 1., 1. - t2) * (rnd2 - rnd13) + rnd13;
	float h3 = smoothstep(0., 1., 1. - t3) * (rnd3 - rnd12) + rnd12;

	float height = outside_crater * (h1 + h2 + h3) / (8. * intPow);

	float n1 = 6. * t1 * (1. - t1) * (rnd1 - rnd23);
	float n2 = 6. * t2 * (1. - t2) * (rnd2 - rnd13);
	float n3 = 6. * t3 * (1. - t3) * (rnd3 - rnd12);

	normal = outside_crater * (n1 * nmldisp1 + n2 * nmldisp2 + n3 * nmldisp3);

	return height;
}

// coefficient of normals "dissolve"
float getDissolve(in float LastLevel, in int Level, in float Multiplier)
{
	float dissolve = int(LastLevel) / LastLevel;
	if (Level >= int(LastLevel))
	{
		dissolve = clamp(Level + 1. - LastLevel, 0., 1.);
		dissolve = cos(rads(90) * dissolve);
	}
	float lvl = 1;//cos(0.5 * rads(180.0) * Level / Count);
	int shift = min(16, 1 + int(LastLevel));
	if (Level >= LastLevel - shift)
	{
		lvl = cos(rads(90) * clamp((Level - (LastLevel - shift)) / shift, 0., 1.));
		lvl = clamp(lvl * lvl - 0.04761904761904767, 0., 1.) * 1.05;
	}
	dissolve *= lvl * (1. - Multiplier) + Multiplier;

	return dissolve;
}

// a lot of parameters have to be calculated for crater, however calculations are pretty straightforward
void initCrater(in vec3 pos, in int vcount, out float vsector_size, out float vang,
				out int vsector_index, out float mult_length, out float hsector_size,
				out int hsector_index, out int vindex)
{
	vsector_size = rads(90) / vcount;                             				// sector size (90 degreese divided by sector count)
	vang = atan(-pos.z, length(pos.xy));                         				// vertical angle
	vsector_index = int(vang / vsector_size);                     				// vertical sector index

	mult_length = cos(vsector_size * (abs(vsector_index) + 0.5)); 				// circle radius at center of sector level
	float circle_length = rads(360) * mult_length;                				// circle length at center of sector level
	int hcount = int(0.5 + circle_length / vsector_size);         				// horizontal sectors count
	if (hcount > 0)
	{
		hsector_size = rads(360) / hcount;                            			// angular size of horizontal sector
		float hang = atan(pos.y, pos.x) + rads(180);                 			// angle of pos in OXY plane
		hsector_index = int(hang / hsector_size);                     			// horizontal sector index
		vindex = vcount + ((vang >= 0) ? vsector_index + 1 : -vsector_index); 	// vertical index
	}
}

// calculates smooth noise at any position on sphere (height and normal for current point)
vec4 height_map(in vec3 Pos, in int Start, in float Count, in float Multiplier)
{
	// bumps part
	// detect triangle of icosaedr and rotate it to convenient coordinate system
	vec3 localpos = vec3(0., 0., 0.);
	vec3 localnormal = vec3(0., 0., 0.);
	float rotZang = 0.;
	float rotYang = 0.;
	vec2 rotYdisp = vec2(0., 0.);
	vec3 pos = normalize(Pos);
	recognizeIcoTriangle(pos, localpos, localnormal, rotZang, rotYang, rotYdisp);

	// prepare cycling by levels of details
	vec3 avgnormal1 = vec3(0., 0., 0.);
	vec3 avgnormal2 = vec3(0., 0., 0.);
	float height = 0.5;
	float outside_crater = 1.;
	int intPow = getIntPow(Start);
	for (int level = 0; level < 1 + int(Count); level ++)
	{
		// get position inside triangles grid
		vec2 cr1 = vec2(0., 0.), cr2 = vec2(0., 0.), cr3 = vec2(0., 0.);
		vec2 frac = localPosToGrid(localpos, intPow, cr1, cr2, cr3);
		// get "global" coordinates for triangle in the grid to get their heights, etc
		vec3 pos1 = vec3(0., 0., 0.), pos2 = vec3(0., 0., 0.), pos3 = vec3(0., 0., 0.);
		vec3 nmldisp1 = vec3(0., 0., 0.), nmldisp2 = vec3(0., 0., 0.), nmldisp3 = vec3(0., 0., 0.);
		gridToGlobal(cr1, localpos, intPow, rotYdisp, rotYang, rotZang, pos1, nmldisp1);
		gridToGlobal(cr2, localpos, intPow, rotYdisp, rotYang, rotZang, pos2, nmldisp2);
		gridToGlobal(cr3, localpos, intPow, rotYdisp, rotYang, rotZang, pos3, nmldisp3);

		// heights for triangle in the grid vertices
		float rnd1 = random3D(pos1);
		float rnd2 = random3D(pos2);
		float rnd3 = random3D(pos3);
// 		rnd1 = 1.0 - rnd1 * rnd1;
// 		rnd2 = 1.0 - rnd2 * rnd2;
// 		rnd3 = 1.0 - rnd3 * rnd3;

		vec3 partial_normal = vec3(0., 0., 0.);
		height += calculateHeightAndNormal(rnd1, rnd2, rnd3, intPow, frac, outside_crater,
											nmldisp1, nmldisp2, nmldisp3, partial_normal);
		// dissolve and strength should also be applyed to partial_normal to improve result
		float dissolve = getDissolve(Start + Count, level, Multiplier);
		float strength = (0 == level) ? 2. : 1.;
		avgnormal1 += localnormal + dissolve * strength * partial_normal;

		// craters
		vec3 nml = pos;                                                     	// normal by default
		if (level > 1 && intPow >= 4 && Count > 0)
		{
			int vcount = intPow;                                                // vectical sectors count
			float vsector_size, vang, mult_length, hsector_size;                // calculate parameters needed for crater
			int vsector_index, hsector_index, vindex;
			initCrater(pos, vcount, vsector_size, vang, vsector_index, mult_length, hsector_size, hsector_index, vindex);
			// if (abs(hsector_index) + 2 < hcount)
			{
				float levelKoef = clamp(level / Count, 0., 1.);                     // level -> 0..1
				float crater_chance = smoothstep(levelKoef, 0., 1.) * 0.3 + 0.1;    // chance of crater present in sector
				if (random2D(rads(180.0) * vec2(hsector_index, vindex)) < crater_chance)
				{
					float signvang = sign(vang);
					signvang = (0 == signvang) ? 1. : signvang;
					vec2 center = vec2((hsector_index + 0.5) * hsector_size,        	// sector center
										(vsector_index + 0.5 * signvang) * vsector_size);

					vec3 local_coords = normalize(vec3(rotate2D(pos.xy, -center.x), pos.z)); // rotate coordinates so that
					local_coords.xz = rotate2D(local_coords.xz, -center.y);         	// sector center get close to zero (local coordinates)
					vec2 scale = vec2(hsector_size * mult_length, vsector_size);    	// scale to get crater coordinates inside -1..1 range
					float t = cos(rads(90) * abs(vsector_index / (vcount - 1.)));
					if (0 != scale.x && 0 != scale.y && t > 0)
					{
						vec2 scaledyz = local_coords.yz / scale;                    	// scaled coordinates

						float r = exp(0.15 * log(t)) * (0.495 - 0.425) + 0.425;     	// max dist between center and sector borders
						vec2 ofs = 0.3 * r * (0.2 + 0.8 * vec2(random2D(center + 13.), random2D(center + 37.)));// displacement from center of sector

						vec2 nmldir = scaledyz - ofs;                               	// 2D vector between center of sector and current point
						float radius = r - max(ofs.x, ofs.y) * 1.2;                 	// crater radius (crater shouldn't get out of sector)
						float nmldirlen = length(nmldir);                           	// length of vector should be less than radius
						if (radius > 0.0 && nmldirlen * 0.96 < radius)                  // if current point is inside circle
						{
							// shape of craters differs depending on their size
							// so it is possible to introduce coefficient to have smooth transition
							float shape = 1. - levelKoef;//random2D(center + 11.) - 0.5;
							height += 0.4 * g(nmldirlen, shape, radius) / intPow;       // g is height of crater, is added to ground height

							float nmltan = 0.4 * gdiff2(nmldirlen, shape, radius);
							nml = normalize(vec3(sign(local_coords.x), -nmltan * normalize(nmldir) * scale * dissolve));	//
							nml.xz = rotate2D(nml.xz, center.y);
							nml.xy = rotate2D(nml.xy, center.x);                        // transform normal back to "global" coordinates
							float roughness = 0.5 + 0.3 * smoothstep(level / Count, 0., 1.);// mark place outside crater for better heights mixing
							float t = smoothstep(1. - clamp(nmldirlen / radius, 0.8, 1.) - 0.8, 0., 1.);
							outside_crater = min(outside_crater, t * (1. - roughness) * 5. + roughness);
						}
					}
				}
			}
		}
		avgnormal2 += normalize(nml);

		intPow *= 2;
	}

	avgnormal1.xz = rotate2D(avgnormal1.xz, -rotYang);                          // transform normals back to "global" coordinates
	avgnormal1.xy = rotate2D(avgnormal1.xy, -rotZang);
	avgnormal1 = 2.0 * (normalize(avgnormal1) - pos) + pos;                    // weight of normals
	avgnormal2 = 32.0 * (normalize(avgnormal2) - pos) + pos;
	vec3 avgnormal = normalize(normalize(avgnormal1) + normalize(avgnormal2));  // mix normals of ground and crater

	return vec4(avgnormal, height); // return height and normal
}
