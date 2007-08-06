#ifndef __JS_TRIMESH_H__
#define __JS_TRIMESH_H__

#include "jscol_triangle.h"
#include <vector>


namespace js
{


//class TriMesh;

/*class VertexIterator
{
public:
	inline VertexIterator(TriMesh* mesh_, const TriMesh::const_tri_iterator& tri_iter_);
	inline VertexIterator(const VertexIterator& rhs);
	inline ~VertexIterator();


	inline VertexIterator& operator = (const VertexIterator& rhs);
	inline bool operator != (const VertexIterator& rhs);
	inline VertexIterator& operator ++ ();
	inline const Vec3& operator * ();
private:
	TriMesh* mesh;
	const_tri_iterator* tri_iter;//nasty, need to find solution
	int vert;
};*/




class TriMesh
{
public:
	TriMesh(){}
	TriMesh(const std::vector<Triangle>& starttris){ tris = starttris; }

	TriMesh(const TriMesh& rhs){ tris = rhs.tris; }
	~TriMesh(){}


	void insertTri(const Triangle& newtri){ tris.push_back(newtri); }

	const int getNumTris() const { return tris.size(); }


	typedef std::vector<Triangle>::const_iterator const_tri_iterator;

	const const_tri_iterator trisBegin() const { return tris.begin(); }
	const const_tri_iterator trisEnd() const { return tris.end(); }

	const std::vector<Triangle>& getTris() const { return tris; }

//	typedef VertexIterator vertex_iterator;

//	vertex_iterator vertsBegin(){ return VertexIterator(this, trisBegin()); }
//	vertex_iterator vertsEnd(){ return VertexIterator(this, trisEnd()); }

private:
	std::vector<Triangle> tris;
};


/*

VertexIterator::VertexIterator(TriMesh* mesh_, const TriMesh::const_tri_iterator& tri_iter_)
{
	tri_iter = new TriMesh::const_tri_iterator(tri_iter_);
	mesh = mesh_;
	vert = 0;
}

VertexIterator::VertexIterator(const VertexIterator& rhs)
{
	*this = rhs;
}

inline ~VertexIterator()
{
	delete tri_iter;
}

VertexIterator::VertexIterator& operator = (const VertexIterator& rhs)
{
	mesh = rhs.mesh;
	tri_iter = new TriMesh::const_tri_iterator(*rhs.tri_iter);
	vert = rhs.vert;
}

bool VertexIterator::operator != (const VertexIterator& rhs)
{
	assert(mesh == rhs.mesh);

	if(*tri_iter != *rhs.tri_iter)
		return true;

	if(vert != rhs.vert)
		return true;

	return false;
}

VertexIterator& VertexIterator::operator ++ ()
{
	vert++;
	if(vert == 3)
	{
		vert = 0;
		(*tri_iter)++;
	}
}

const Vec3& VertexIterator::operator * ()
{
		return (*(*tri_iter)).getVertex(vert);
}
*/

}//end namespace js
#endif //__JS_TRIMESH_H__
