/*
 * Copyright (c) 2009, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "Sampler.h"
#include "MathUtil.h"
#include <math.h>
#include <gdk/gdk.h>


float sampler_falloff_none(float distance) {
	return 1;
}

float sampler_falloff_linear(float distance) {
	return 1-distance;
}

float sampler_falloff_quadratic(float distance) {
	return 1-(distance*distance);
}

float sampler_falloff_cubic(float distance) {
	return 1-(distance*distance*distance);
}

float sampler_falloff_exponential(float distance) {
	return 1/exp(5*distance*distance);
}

struct Sampler* sampler_new() {
	struct Sampler* sampler = new struct Sampler;
	sampler->oversample=0;
	sampler_set_falloff(sampler, NONE);
	return sampler;
}

void sampler_destroy(struct Sampler *sampler) {
	delete sampler;
}


void sampler_set_falloff(struct Sampler *sampler, enum SamplerFalloff falloff) {
	sampler->falloff = falloff;
	switch (falloff){
	case NONE:
		sampler->falloff_fnc = sampler_falloff_none;
		break;
	case LINEAR:
		sampler->falloff_fnc = sampler_falloff_linear;
		break;
	case QUADRATIC:
		sampler->falloff_fnc = sampler_falloff_quadratic;
		break;
	case CUBIC:
		sampler->falloff_fnc = sampler_falloff_cubic;
		break;
	case EXPONENTIAL:
		sampler->falloff_fnc = sampler_falloff_exponential;
		break;
	default:
		sampler->falloff_fnc = 0;
	}
}

void sampler_set_oversample(struct Sampler *sampler, int oversample) {
	sampler->oversample = oversample;
}

int sampler_get_color_sample(struct Sampler *sampler, Color* color) {
	Color sample;
	Color result;
	float divider = 0;

	color_zero(&result);

	GdkWindow* root_window;
	GdkImage* section;
	GdkModifierType state;

	root_window = gdk_get_default_root_window();

	int x, y;

	gdk_window_get_pointer(root_window, &x, &y, &state);

	int width, height;

	gdk_window_get_geometry(root_window, NULL, NULL, &width, &height, NULL);

	int left, right, top, bottom;

	left = max_int(0, x - sampler->oversample);
	right = min_int(width, x + sampler->oversample + 1);
	top = max_int(0, y - sampler->oversample);
	bottom = min_int(height, y + sampler->oversample + 1);
	width = right - left;
	height = bottom - top;

	section = gdk_drawable_get_image (root_window, left, top, width, height);

	guint32 color_value;
	GdkColor gdk_color;

	int center_x=x-left;
	int center_y=y-top;

	GdkColormap* colormap=gdk_colormap_get_system();

	float max_distance = sqrt(2*pow(sampler->oversample, 2));

	for (int x=-sampler->oversample; x<=sampler->oversample; ++x){
		for (int y=-sampler->oversample; y<=sampler->oversample; ++y){

			if ((center_x+x<0) || (center_y+y<0)) continue;
			if ((center_x+x>=width) || (center_y+y>=height)) continue;

			color_value = gdk_image_get_pixel(section, center_x+x, center_y+y);
			gdk_colormap_query_color(colormap, color_value, &gdk_color);

			sample.rgb.red=gdk_color.red/(float)0xFFFF;
			sample.rgb.green=gdk_color.green/(float)0xFFFF;
			sample.rgb.blue=gdk_color.blue/(float)0xFFFF;

			float f;
			if (sampler->oversample){
				f = sampler->falloff_fnc(sqrt(x * x + y * y) / max_distance);
			}else{
				f = 1;
			}
			color_multiply(&sample, f);
			color_add(&result, &sample);
			divider+=f;

		}
	}

	color_multiply(&result, 1/divider);

	color_copy(&result, color);

	g_object_unref (section);

	return 0;
}

enum SamplerFalloff sampler_get_falloff(struct Sampler *sampler)
{
	return sampler->falloff;
}
int sampler_get_oversample(struct Sampler *sampler) {
	return sampler->oversample;
}
