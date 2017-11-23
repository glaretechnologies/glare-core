/*=====================================================================
BVHBuilder.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#include "BVHBuilder.h"


BVHBuilder::~BVHBuilder()
{}


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
