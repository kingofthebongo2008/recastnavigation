//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "MeshLoaderObj.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <vector>

rcMeshLoaderObj::rcMeshLoaderObj() :
	m_scale(1.0f),
	m_verts(0),
	m_tris(0),
	m_normals(0),
	m_vertCount(0),
	m_triCount(0)
{
}

rcMeshLoaderObj::~rcMeshLoaderObj()
{
}
		
void rcMeshLoaderObj::addVertex(float x, float y, float z, int& cap)
{
	m_verts.push_back(x);
	m_verts.push_back(y);
	m_verts.push_back(z);
	m_vertCount++;
}

void rcMeshLoaderObj::addTriangle(int a, int b, int c, int& cap)
{
	m_tris.push_back(a);
	m_tris.push_back(b);
	m_tris.push_back(c);
	m_triCount++;
}

static char* parseRow(char* buf, char* bufEnd, char* row, int len)
{
	bool start = true;
	bool done = false;
	int n = 0;
	while (!done && buf < bufEnd)
	{
		char c = *buf;
		buf++;
		// multirow
		switch (c)
		{
			case '\\':
				break;
			case '\n':
				if (start) break;
				done = true;
				break;
			case '\r':
				break;
			case '\t':
			case ' ':
				if (start) break;
				// else falls through
			default:
				start = false;
				row[n++] = c;
				if (n >= len-1)
					done = true;
				break;
		}
	}
	row[n] = '\0';
	return buf;
}

static int parseFace(char* row, int* data, int n, int vcnt)
{
	int j = 0;
	while (*row != '\0')
	{
		// Skip initial white space
		while (*row != '\0' && (*row == ' ' || *row == '\t'))
			row++;
		char* s = row;
		// Find vertex delimiter and terminated the string there for conversion.
		while (*row != '\0' && *row != ' ' && *row != '\t')
		{
			if (*row == '/') *row = '\0';
			row++;
		}
		if (*s == '\0')
			continue;
		int vi = atoi(s);
		data[j++] = vi < 0 ? vi+vcnt : vi-1;
		if (j >= n) return j;
	}
	return j;
}

bool rcMeshLoaderObj::load(const std::string& filename, bool saveAsBinary)
{
	char* buf = 0;
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return false;
	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return false;
	}
	long bufSize = ftell(fp);
	if (bufSize < 0)
	{
		fclose(fp);
		return false;
	}
	if (fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);
		return false;
	}
	buf = new char[bufSize];
	if (!buf)
	{
		fclose(fp);
		return false;
	}
	size_t readLen = fread(buf, bufSize, 1, fp);
	fclose(fp);

	if (readLen != 1)
	{
		delete[] buf;
		return false;
	}

	char* src = buf;
	char* srcEnd = buf + bufSize;
	char row[512];
	int face[32];
	float x,y,z;
	int nv;
	int vcap = 0;
	int tcap = 0;
	
	while (src < srcEnd)
	{
		// Parse one row
		row[0] = '\0';
		src = parseRow(src, srcEnd, row, sizeof(row)/sizeof(char));
		// Skip comments
		if (row[0] == '#') continue;
		if (row[0] == 'v' && row[1] != 'n' && row[1] != 't')
		{
			// Vertex pos
			sscanf(row+1, "%f %f %f", &x, &y, &z);
			addVertex(x, y, z, vcap);
		}
		if (row[0] == 'f')
		{
			// Faces
			nv = parseFace(row+1, face, 32, m_vertCount);
			for (int i = 2; i < nv; ++i)
			{
				const int a = face[0];
				const int b = face[i-1];
				const int c = face[i];
				if (a < 0 || a >= m_vertCount || b < 0 || b >= m_vertCount || c < 0 || c >= m_vertCount)
					continue;
				addTriangle(a, b, c, tcap);
			}
		}
	}

	delete [] buf;

	// Calculate normals.
	m_normals.resize(m_triCount * 3);
	for (int i = 0; i < m_triCount*3; i += 3)
	{
		const float* v0 = &m_verts[m_tris[i]*3];
		const float* v1 = &m_verts[m_tris[i+1]*3];
		const float* v2 = &m_verts[m_tris[i+2]*3];
		float e0[3], e1[3];
		for (int j = 0; j < 3; ++j)
		{
			e0[j] = v1[j] - v0[j];
			e1[j] = v2[j] - v0[j];
		}
		float* n = &m_normals[i];
		n[0] = e0[1]*e1[2] - e0[2]*e1[1];
		n[1] = e0[2]*e1[0] - e0[0]*e1[2];
		n[2] = e0[0]*e1[1] - e0[1]*e1[0];
		float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
		if (d > 0)
		{
			d = 1.0f/d;
			n[0] *= d;
			n[1] *= d;
			n[2] *= d;
		}
	}
	
	m_filename = filename;

	if (saveAsBinary)
	{
		const std::string vertices = filename + ".vertices";
		const std::string indices  = filename + ".indices";
		const std::string normals  = filename + ".normals";	

		FILE* fpV = fopen(vertices.c_str(), "wb");
		{
			if (!fpV)
				return false;

			fwrite(&m_vertCount, sizeof(int), 1, fpV);	
			fwrite(&m_verts[0], sizeof(float) * 3, m_vertCount, fpV);
			fclose(fpV);	
		}

		FILE* fpI = fopen(indices.c_str(), "wb");
		{
			if (!fpI)
				return false;

			fwrite(&m_triCount, sizeof(int), 1, fpI);
			fwrite(&m_tris[0], sizeof(int) * 3, m_triCount, fpI);
			fclose(fpI);
		}
		
		FILE* fpN = fopen(normals.c_str(), "wb");
		{
			if (!fpN)
				return false;

			int normalCount = m_triCount * 3;

			fwrite(&normalCount, sizeof(int), 1, fpN);
			fwrite(&m_normals[0], sizeof(float), normalCount, fpN);
			fclose(fpN);
		}

		//compare
		if ( true )
		{
			{
				FILE* fpV = fopen(vertices.c_str(), "rb");
				{
					std::vector<float> ve;

					if (!fpV)
						return false;

					int vertCount = 0;
					fread(&vertCount, sizeof(int), 1, fpV);

					ve.resize(vertCount * 3);

					fread(&ve[0], sizeof(float) * 3, vertCount, fpV);
					fclose(fpV);

					if (m_verts.size() != ve.size())
					{
						__debugbreak();
					}

					int cmp = std::memcmp(&ve[0], &m_verts[0], vertCount * sizeof(float) * 3);

					if (cmp != 0)
					{
						__debugbreak();
					}
				}
			}

			{
				FILE* fpI = fopen(indices.c_str(), "rb");
				{
					std::vector<int> ind;

					if (!fpI)
						return false;

					int triCount = 0;
					fread(&triCount, sizeof(int), 1, fpI);

					ind.resize(triCount * 3);

					fread(&ind[0], sizeof(int) * 3, triCount, fpI);
					fclose(fpI);

					if (ind.size() != m_tris.size())
					{
						__debugbreak();
					}

					int cmp = std::memcmp(&ind[0], &m_tris[0], triCount * sizeof(int) * 3);

					if (cmp != 0)
					{
						__debugbreak();
					}
				}
			}

			{
				FILE* fpN = fopen(normals.c_str(), "rb");
				{
					std::vector<float> no;

					if (!fpN)
						return false;

					int normalCount = 0;
					fread(&normalCount, sizeof(int), 1, fpN);

					no.resize(normalCount);

					fread(&no[0], sizeof(float), normalCount, fpN);
					fclose(fpN);

					if (no.size() != m_normals.size())
					{
						__debugbreak();
					}

					int cmp = std::memcmp(&no[0], &m_normals[0], normalCount * sizeof(float));

					if (cmp != 0)
					{
						__debugbreak();
					}
				}
			}
		}
	}

	return true;
}

bool rcMeshLoaderObj::loadBinary(const std::string& fileName)
{
	const std::string vertices = fileName + ".vertices";
	const std::string indices = fileName + ".indices";
	const std::string normals = fileName + ".normals";

	{
		FILE* fpV = fopen(vertices.c_str(), "rb");
		{
			std::vector<float> ve;

			if (!fpV)
				return false;

			int vertCount = 0;
			fread(&vertCount, sizeof(int), 1, fpV);

			ve.resize(vertCount * 3);

			fread(&ve[0], sizeof(float) * 3, vertCount, fpV);
			fclose(fpV);

			m_vertCount = vertCount;
			m_verts     = std::move(ve);
		}
	}

	{
		FILE* fpI = fopen(indices.c_str(), "rb");
		{
			std::vector<int> ind;

			if (!fpI)
				return false;

			int triCount = 0;
			fread(&triCount, sizeof(int), 1, fpI);

			ind.resize(triCount * 3);

			fread(&ind[0], sizeof(int) * 3, triCount, fpI);
			fclose(fpI);

			m_triCount = triCount;
			m_tris = std::move(ind);
		}
	}

	{
		FILE* fpN = fopen(normals.c_str(), "rb");
		{
			std::vector<float> no;

			if (!fpN)
				return false;

			int normalCount = 0;
			fread(&normalCount, sizeof(int), 1, fpN);

			no.resize(normalCount);

			fread(&no[0], sizeof(normalCount), normalCount, fpN);
			fclose(fpN);

			m_normals = std::move(no);
		}
	}

	return true;
}
