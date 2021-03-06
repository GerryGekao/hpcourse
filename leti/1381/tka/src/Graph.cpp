
#include "stdafx.h"
#include <tbb/flow_graph.h>
#include "picture.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include "Data.h"

using namespace tbb::flow;
using namespace std;



int main(int argc, char *argv[]) {
	Data info(argc, argv);
	srand(time(NULL));
	graph g;

	int counter = 0;
	auto source = [&info, &counter](Picture& result)
	{
		if (counter >= info.get_number_image())return false;
		result = Picture(counter);
		++counter;
		return true;
	};
	source_node<Picture> input(g, source);
	limiter_node<Picture> limit_node(g, info.get_image_limit());
	broadcast_node<Picture> broad_node(g);
	auto find_max_pixel = [](const Picture& input){return input.max_pixel(); };
	function_node<Picture, Cells> f_find_max(g, unlimited, find_max_pixel);
	auto find_min_pixel = [](const Picture& input){return input.min_pixel(); };
	function_node<Picture, Cells> f_find_min(g, unlimited, find_min_pixel);
	auto find_equil_pixel = [&info](const Picture& input){return input.find_pixel(info.get_brightness()); };
	function_node<Picture, Cells> f_find_equil(g, unlimited, find_equil_pixel);
	join_node<tuple<Picture, Cells, Cells, Cells>, tag_matching> join_vector(g,
		[](const Picture& pt)->int {return pt.get_id();},
		[](const Cells& cl)->int {return cl.first; },
		[](const Cells& cl)->int {return cl.first; },
		[](const Cells& cl)->int {return cl.first; });
	auto distinguish_pixels = [](tuple<Picture, Cells, Cells, Cells> in){
		auto pt = tbb::flow::get<0>(in);
		pt.lead_point(tbb::flow::get<1>(in));
		pt.lead_point(tbb::flow::get<2>(in));
		pt.lead_point(tbb::flow::get<3>(in));
		return pt;
	};
	function_node<tuple<Picture, Cells, Cells, Cells>, Picture> f_dist_pixel(g, unlimited, distinguish_pixels);
	auto inverse_image = [](const Picture& input)
	{
		Picture new_im(input);
		new_im.inverse_image();
		return new_im;
	};
	function_node<Picture, Picture> f_invers_image(g, unlimited, inverse_image);
	auto find_mean_brightness = [](const Picture& input){	return pair<int, double>(input.get_id(),input.mean_brightness());};
	function_node<Picture, pair<int,double>> f_find_mean(g, unlimited, find_mean_brightness);
	ofstream out_file(info.get_file_name());
	auto file_output = [&out_file, &info](pair<int, double> b){if (parser.get_file_name() != "") out_file << b.second << endl;return b.first; };
	function_node<pair<int,double>,int> f_file_output(g, serial, file_output);
	join_node<tuple<int, Picture>,tag_matching> finish_join(g,
		[](const int& id)->int {return id; },
		[](const Picture& pt)->int {return pt.get_id(); });
	function_node<tuple<int, Picture>> finish_image(g, serial, [](tuple<int, Picture> out ){cout << "result_image = "<< std::get<0>(out) <<" = " << std::get<1>(out).get_id()<< endl; });
	make_edge(input, limit_node);
	make_edge(limit_node, broad_node);
	make_edge(broad_node, f_find_max);
	make_edge(broad_node, f_find_min);
	make_edge(broad_node, f_find_equil);
	make_edge(f_find_max, input_port<1>(join_vector));
	make_edge(f_find_min, input_port<2>(join_vector));
	make_edge(f_find_equil, input_port<3>(join_vector));
	make_edge(broad_node, input_port<0>(join_vector));
	make_edge(join_vector, f_dist_pixel);
	make_edge(f_dist_pixel, f_invers_image);
	make_edge(f_dist_pixel, f_find_mean);
	make_edge(f_find_mean, f_file_output);
	make_edge(f_file_output, input_port<0>(finish_join));
	make_edge(f_invers_image, input_port<1>(finish_join));
	make_edge(finish_join, finish_image);
	make_edge(finish_image, limit_node.decrement);

	g.wait_for_all();
	return 0;
}
