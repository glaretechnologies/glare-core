/*=====================================================================
BVHBuilder.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#include "BVHBuilder.h"


#include "../maths/vec2.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/Plotter.h"


BVHBuilder::~BVHBuilder()
{}


/*static int numLeavesInSubTree(const js::Vector<ResultNode, 64>& nodes, const ResultNode& node)
{
	if(node.interior)
		return numLeavesInSubTree(nodes, nodes[node.left]) + numLeavesInSubTree(nodes, nodes[node.right]);
	else
		return node.right - node.left;
}*/


struct SAHStats
{
	SAHStats() : sum_sum_left_right_prob(0), sum_left_right_prob_N(0), sum_num_leaf_obs(0), sum_num_leaf_obs_N(0) {}

	std::vector<Vec2f> data;
	double sum_sum_left_right_prob;
	int sum_left_right_prob_N;

	double sum_num_leaf_obs;
	int sum_num_leaf_obs_N;
};


/*static float expectedNumNodesTraversedForNumObs(int obs)
{
	const float av_num_obs_per_leaf = 2.5096484422267937;
	const float nodes = (float)obs  / av_num_obs_per_leaf; // Expected num nodes in subtree

	const int expected_depth = (int)logBase2(nodes);
	//float sum = 0;
	//for(int i=0; i<

	const float av_P_A_plus_B = 1.26778367717;
	return expected_depth + pow(av_P_A_plus_B, expected_depth);
}*/


static float doGetSAHCost(const js::Vector<ResultNode, 64>& nodes, float intersection_cost, const ResultNode& node, int depth, SAHStats& stats)
{
	const float traversal_cost = 1.f;
	if(node.interior)
	{
		// Get the actual SAH cost, based on tree traversal
		const float left_prob  = nodes[node.left ].aabb.getSurfaceArea() / node.aabb.getSurfaceArea();
		const float right_prob = nodes[node.right].aabb.getSurfaceArea() / node.aabb.getSurfaceArea();
		const float actual_cost = traversal_cost + left_prob * doGetSAHCost(nodes, intersection_cost, nodes[node.left], depth + 1, stats) + right_prob * doGetSAHCost(nodes, intersection_cost, nodes[node.right], depth + 1, stats);

#if 0
		// Get estimated cost, as it would be during construction
		const int num_left  = numLeavesInSubTree(nodes, nodes[node.left]);
		const int num_right = numLeavesInSubTree(nodes, nodes[node.right]);
		const float estimated_cost = traversal_cost + left_prob * num_left + right_prob * num_right;

		const float av_P_A_plus_B = 1.2;// 26778367717;
		const float av_num_obs_per_leaf = 2.5096484422267937;
		const float expected_depth_left  = logBase2(num_left  / av_num_obs_per_leaf);
		const float expected_depth_right = logBase2(num_right / av_num_obs_per_leaf);

		const float expected_num_leaves_traversed_left  = pow(av_P_A_plus_B, expected_depth_left);
		const float expected_num_leaves_traversed_right = pow(av_P_A_plus_B, expected_depth_right);

		const float expected_num_traversals_left  = expected_depth_left  + expected_num_leaves_traversed_left;
		const float expected_num_traversals_right = expected_depth_right + expected_num_leaves_traversed_right;

		const float better_est_cost_left  = left_prob  * (expected_num_leaves_traversed_left  * av_num_obs_per_leaf * intersection_cost + expected_num_traversals_left  * traversal_cost);
		const float better_est_cost_right = right_prob * (expected_num_leaves_traversed_right * av_num_obs_per_leaf * intersection_cost + expected_num_traversals_right * traversal_cost);

		//const float better_est_cost_left  = left_prob  * (expected_depth_left +  pow(av_P_A_plus_B, expected_depth_left)  * (/*traversal cost*/1.f + intersection_cost * av_num_obs_per_leaf);
		//const float better_est_cost_right = right_prob * (expected_depth_right + pow(av_P_A_plus_B, expected_depth_right) * (/*traversal cost*/1.f + intersection_cost * av_num_obs_per_leaf);

		const float better_est_cost = better_est_cost_left + better_est_cost_right + traversal_cost;

		const float sum_left_right_prob = left_prob + right_prob;
		//printVar(sum_left_right_prob);
		stats.sum_sum_left_right_prob += sum_left_right_prob;
		stats.sum_left_right_prob_N++;

		if(depth < 14)
		{
			/*if(depth < 5)
			{
				conPrint("");
				conPrint("Depth: " + toString(depth));
				conPrint("actual_cost:    " + toString(actual_cost));
				//conPrint("estimated_cost: " + toString(estimated_cost));
				conPrint("better_est_cost: " + toString(better_est_cost));
				printVar(sum_left_right_prob);
			}
			stats.data.push_back(Vec2f(actual_cost, better_est_cost));*/
		}
#endif
		return actual_cost;
	}
	else
	{
		const int num_leaf_obs = node.right - node.left;

		stats.sum_num_leaf_obs += num_leaf_obs;
		stats.sum_num_leaf_obs_N++;

		return intersection_cost * num_leaf_obs;
	}
}


float BVHBuilder::getSAHCost(const js::Vector<ResultNode, 64>& nodes, float intersection_cost)
{
	SAHStats stats;
	const float cost =  doGetSAHCost(nodes, intersection_cost, nodes[0], /*depth=*/0, stats);

	//const double av_sum_left_right_prob = stats.sum_sum_left_right_prob / stats.sum_left_right_prob_N;
	//printVar(av_sum_left_right_prob);
	//
	//const double av_num_leaf_obs= stats.sum_num_leaf_obs / stats.sum_num_leaf_obs_N;
	//printVar(av_num_leaf_obs);


	std::vector<Plotter::DataSet> datasets(2);
	datasets[0].points = stats.data;
	//datasets[0].points.push_back(Vec2f(1, 1));

	datasets[1].key = "y=x";
	for(float x=0.1f; x<80; x += 0.1)
		datasets[1].points.push_back(Vec2f(x, x));

	//datasets[2].key = "fit";
	//for(float x=30.f; x<50; x += 0.1)
	//	datasets[2].points.push_back(Vec2f(x, pow(2, x - 31)));

	//Plotter::PlotOptions options;
	////options.y_axis_log = true;
	//Plotter::scatterPlot("SAH.png", "SAH", "actual cost", "estimated cost", datasets, options);

	return cost;
}


//void BVHBuilder::printResultNode(const ResultNode& result_node)
//{
//	if(result_node.interior)
//		conPrint(" Interior");
//	else
//		conPrint(" Leaf");
//
//	conPrint("	AABB:  " + result_node.aabb.toStringNSigFigs(4));
//	if(result_node.interior)
//	{
//		conPrint("	left_child_index:  " + toString(result_node.left));
//		conPrint("	right_child_index: " + toString(result_node.right));
//		conPrint("	right_child_chunk_index: " + toString(result_node.right_child_chunk_index));
//	}
//	else
//	{
//		conPrint("	objects_begin: " + toString(result_node.left));
//		conPrint("	objects_end:   " + toString(result_node.right));
//	}
//}
//
//void BVHBuilder::printResultNodes(const js::Vector<ResultNode, 64>& result_nodes)
//{
//	for(size_t i=0; i<result_nodes.size(); ++i)
//	{
//		conPrintStr("node " + toString(i) + ": ");
//		printResultNode(result_nodes[i]);
//	}
//}
