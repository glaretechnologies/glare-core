/*=====================================================================
TopologicalSort.h
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <map>
#include <set>
#include <vector>
#include <assert.h>
#include <string.h>


/*=====================================================================
TopologicalSort
---------------
Uses something like Kahn's algorithm, see See https://en.wikipedia.org/wiki/Topological_sorting
Instead of operating on a graph however, we will use maps to sets.
=====================================================================*/
namespace TopologicalSort
{

// Places the elements [begin, end) into topologically sorted order, if there are no cycles in the graph, and returns true.
// If there is a cycle, returns false, and the elements [begin, end) are unchanged.
// 
// follows_pairs: Contains a pair (a, b) if a follows b, e.g. a should come after b in the resulting sorted array.
// For example, if follows_map contains an entry (2, 1), then 2 should come after 1.

template <class ValueType, class IterType>
static bool topologicalSort(IterType begin, IterType end, const std::vector<std::pair<ValueType, ValueType>>& follows_pairs)
{
	std::map<ValueType, std::set<ValueType>> follows_map; // Contains an entry (a, b) if a follows b, e.g. a should come after b in the resulting sorted array.
	std::map<ValueType, std::set<ValueType>> preceeds_map; // Contains an entry (a, b) if a preceeds b, e.g. b follows a.

	// Build follows_map and preceeds map
	for(auto it = follows_pairs.begin(); it != follows_pairs.end(); ++it)
	{
		follows_map [it->first ].insert(it->second);
		preceeds_map[it->second].insert(it->first);
	}

	std::vector<ValueType> sorted_elements; // Empty list that will contain the sorted elements (L)
	std::vector<ValueType> S; // Set of all nodes with no incoming edge

	// Compute S: nodes with no incoming edge = nodes that don't follow anything
	for(IterType i = begin; i != end; ++i)
		if(follows_map.count(*i) == 0)
			S.push_back(*i);

	while(!S.empty())
	{
		// remove a node n from S
		const ValueType n = S.back();
		S.pop_back();

		// Add n to L 
		sorted_elements.push_back(n);

		// for each node m with an edge e from n to m do  (e.g. for each node m such that n proceeds m, e.g. for each node m such that m follows n)
		const auto n_res = preceeds_map.find(n);
		if(n_res != preceeds_map.end()) // If there are some elements (n, x) in proceeds map:
		{
			// remove edge e from the graph:
			// Removal from preceeds_map is done below outside the loop.
			
			std::set<ValueType>& vals = n_res->second;

			for(auto it = vals.begin(); it != vals.end(); ++it) // Values x in from elements (n, x) in proceeds map
			{
				const ValueType& m = *it;

				// Remove from follows map, want to remove element (m, n)
				assert(follows_map.find(m) != follows_map.end());
				assert(follows_map.find(m)->second.count(n) > 0);

				auto follows_m_it = follows_map.find(m);
				std::set<ValueType>& follows_m_set = follows_m_it->second;
				follows_m_set.erase(n);
				if(follows_m_set.empty()) // If set is empty, remove from map.  There are now no elements in follows map (m, x) for any x.
				{
					follows_map.erase(follows_m_it);
					S.push_back(m); // if m has no other incoming edges then insert m into S.  (e.g. if there is no element (m, x) in follows map)
				}
			}

			preceeds_map.erase(n_res); // Remove all elements with key n.
		}
	}

	// If graph has edges then graph has at least one cycle.
	if(follows_map.empty())
	{
		assert(preceeds_map.empty());
		assert(sorted_elements.size() == (size_t)(end - begin));

		// Copy sorted_elements to input/output array
		auto sorted_it = sorted_elements.begin();
		for(auto it = begin; it != end; ++it)
			*it = *sorted_it++;

		return true;
	}
	else
		return false; // We have a cycle
}


void test();

	
} // end namespace TopologicalSort
