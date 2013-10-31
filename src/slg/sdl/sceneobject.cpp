/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <boost/format.hpp>

#include "slg/sdl/sceneobject.h"
#include "luxrays/core/extmeshcache.h"

using namespace luxrays;
using namespace slg;

void SceneObject::AddReferencedMeshes(boost::unordered_set<const luxrays::ExtMesh *> &referencedMesh) const {
	referencedMesh.insert(mesh);

	// Check if it is an instance and add referenced mesh
	if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;
		referencedMesh.insert(imesh->GetExtTriangleMesh());
	}	
}

void SceneObject::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	if (mat == oldMat)
		mat = newMat;
}

Properties SceneObject::ToProperties(const ExtMeshCache &extMeshCache) const {
	Properties props;

	const std::string name = GetName();
	props.Set(Property("scene.objects." + name + ".material")(mat->GetName()));
	props.Set(Property("scene.objects." + name + ".ply")(
			"mesh-" + (boost::format("%05d") % extMeshCache.GetExtMeshIndex(mesh)).str() + ".ply"));

	if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		const ExtInstanceTriangleMesh *inst = (const ExtInstanceTriangleMesh *)mesh;
		props << Property("scene.objects." + name + ".transformation")(inst->GetTransformation().m);
	}

	return props;
}

//------------------------------------------------------------------------------
// SceneObjectDefinitions
//------------------------------------------------------------------------------

SceneObjectDefinitions::SceneObjectDefinitions() { }

SceneObjectDefinitions::~SceneObjectDefinitions() {
	BOOST_FOREACH(SceneObject *o, objs)
		delete o;
}

void SceneObjectDefinitions::DefineSceneObject(const std::string &name, SceneObject *newObj) {
	if (IsSceneObjectDefined(name)) {
		const SceneObject *oldObj = GetSceneObject(name);

		// Update name/SceneObject definition
		const u_int index = GetSceneObjectIndex(name);
		objs[index] = newObj;
		objsByName.erase(name);
		objsByName.insert(std::make_pair(name, newObj));

		// Delete old SceneObject
		delete oldObj;
	} else {
		// Add the new SceneObject
		objs.push_back(newObj);
		objsByName.insert(std::make_pair(name, newObj));
	}
}

const SceneObject *SceneObjectDefinitions::GetSceneObject(const std::string &name) const {
	// Check if the SceneObject has been already defined
	std::map<std::string, SceneObject *>::const_iterator it = objsByName.find(name);

	if (it == objsByName.end())
		throw std::runtime_error("Reference to an undefined SceneObject: " + name);
	else
		return it->second;
}

SceneObject *SceneObjectDefinitions::GetSceneObject(const std::string &name) {
	// Check if the SceneObject has been already defined
	std::map<std::string, SceneObject *>::const_iterator it = objsByName.find(name);

	if (it == objsByName.end())
		throw std::runtime_error("Reference to an undefined SceneObject: " + name);
	else
		return it->second;
}

u_int SceneObjectDefinitions::GetSceneObjectIndex(const std::string &name) const {
	return GetSceneObjectIndex(GetSceneObject(name));
}

u_int SceneObjectDefinitions::GetSceneObjectIndex(const SceneObject *m) const {
	for (u_int i = 0; i < objs.size(); ++i) {
		if (m == objs[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined SceneObject: " + boost::lexical_cast<std::string>(m));
}

std::vector<std::string> SceneObjectDefinitions::GetSceneObjectNames() const {
	std::vector<std::string> names;
	names.reserve(objs.size());
	for (std::map<std::string, SceneObject *>::const_iterator it = objsByName.begin(); it != objsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void SceneObjectDefinitions::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	// Replace old material direct references with new one
	BOOST_FOREACH(SceneObject *o, objs)
		o->UpdateMaterialReferences(oldMat, newMat);
}

void SceneObjectDefinitions::DeleteSceneObject(const std::string &name) {
	const u_int index = GetSceneObjectIndex(name);
	objs.erase(objs.begin() + index);
	objsByName.erase(name);
}
