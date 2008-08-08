/*
---------------------------------------------------------------------------
Open Asset Import Library (ASSIMP)
---------------------------------------------------------------------------

Copyright (c) 2006-2008, ASSIMP Development Team

All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the following 
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the ASSIMP team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the ASSIMP Development Team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file Implementation of the post processing step to invert
 * all normals in meshes with infacing normals.
 */

// CRT includes
#include <vector>
#include <assert.h>

// internal headers
#include "FixNormalsStep.h"
#include "SpatialSort.h"

// public ASSIMP headers
#include "../include/DefaultLogger.h"
#include "../include/aiPostProcess.h"
#include "../include/aiMesh.h"
#include "../include/aiScene.h"

using namespace Assimp;

#ifdef _MSC_VER
#	define sprintf sprintf_s
#endif

// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
FixInfacingNormalsProcess::FixInfacingNormalsProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well
FixInfacingNormalsProcess::~FixInfacingNormalsProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Returns whether the processing step is present in the given flag field.
bool FixInfacingNormalsProcess::IsActive( unsigned int pFlags) const
{
	return (pFlags & aiProcess_FixInfacingNormals) != 0;
}

// ------------------------------------------------------------------------------------------------
// Executes the post processing step on the given imported data.
void FixInfacingNormalsProcess::Execute( aiScene* pScene)
{
	DefaultLogger::get()->debug("FixInfacingNormalsProcess begin");

	bool bHas = false;
	for( unsigned int a = 0; a < pScene->mNumMeshes; a++)
		if(ProcessMesh( pScene->mMeshes[a],a))bHas = true;

	if (bHas)DefaultLogger::get()->debug("FixInfacingNormalsProcess finished. At least one mesh' normals have been flipped.");
	else DefaultLogger::get()->debug("FixInfacingNormalsProcess finished");
}

// ------------------------------------------------------------------------------------------------
// Apply the step to the mesh
bool FixInfacingNormalsProcess::ProcessMesh( aiMesh* pcMesh, unsigned int index)
{
	ai_assert(NULL != pcMesh);

	if (!pcMesh->HasNormals())return false;

	// compute the bounding box of both the model vertices + normals and
	// the umodified model vertices. Then check whether the first BB
	// is smaller than the second. In this case we can assume that the
	// normals need to be flipped, although there are a few special cases ..
	// convex, concave, planar models ...

	aiVector3D vMin0(1e10f,1e10f,1e10f);
	aiVector3D vMin1(1e10f,1e10f,1e10f);
	aiVector3D vMax0(-1e10f,-1e10f,-1e10f);
	aiVector3D vMax1(-1e10f,-1e10f,-1e10f);

	for (unsigned int i = 0; i < pcMesh->mNumVertices;++i)
	{
		vMin1.x = std::min(vMin1.x,pcMesh->mVertices[i].x);
		vMin1.y = std::min(vMin1.y,pcMesh->mVertices[i].y);
		vMin1.z = std::min(vMin1.z,pcMesh->mVertices[i].z);

		vMax1.x = std::max(vMax1.x,pcMesh->mVertices[i].x);
		vMax1.y = std::max(vMax1.y,pcMesh->mVertices[i].y);
		vMax1.z = std::max(vMax1.z,pcMesh->mVertices[i].z);

		aiVector3D vWithNormal = pcMesh->mVertices[i] + pcMesh->mNormals[i];

		vMin0.x = std::min(vMin0.x,vWithNormal.x);
		vMin0.y = std::min(vMin0.y,vWithNormal.y);
		vMin0.z = std::min(vMin0.z,vWithNormal.z);

		vMax0.x = std::max(vMax0.x,vWithNormal.x);
		vMax0.y = std::max(vMax0.y,vWithNormal.y);
		vMax0.z = std::max(vMax0.z,vWithNormal.z);
	}

	const float fDelta0_x = (vMax0.x - vMin0.x);
	const float fDelta0_y = (vMax0.y - vMin0.y);
	const float fDelta0_z = (vMax0.z - vMin0.z);

	const float fDelta1_x = (vMax1.x - vMin1.x);
	const float fDelta1_y = (vMax1.y - vMin1.y);
	const float fDelta1_z = (vMax1.z - vMin1.z);

	// check the case where the boxes are overlapping
	if (fDelta0_x > 0.0f != fDelta1_x > 0.0f)return false;
	if (fDelta0_y > 0.0f != fDelta1_y > 0.0f)return false;
	if (fDelta0_z > 0.0f != fDelta1_z > 0.0f)return false;


	// check the case of a very planar surface
	const float fDelta1_yz = fDelta1_y * fDelta1_z;

	if (fDelta1_x < 0.05f * sqrtf( fDelta1_yz ))return false;
	if (fDelta1_y < 0.05f * sqrtf( fDelta1_z * fDelta1_x ))return false;
	if (fDelta1_z < 0.05f * sqrtf( fDelta1_y * fDelta1_x ))return false;

	// now compare the volume of the bounding boxes
	if (::fabsf(fDelta0_x * fDelta1_yz) <
		::fabsf(fDelta1_x * fDelta1_y * fDelta1_z))
	{
		if (!DefaultLogger::isNullLogger())
		{
			char buffer[128]; // should be sufficiently large
			::sprintf(buffer,"Mesh %i: Normals are facing inwards (or the mesh is planar)",index);
			DefaultLogger::get()->info(buffer);
		}

		for (unsigned int i = 0; i < pcMesh->mNumVertices;++i)
		{
			pcMesh->mNormals[i] *= -1.0f;
		}
		return true;
	}
	return false;
}