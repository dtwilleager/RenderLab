#pragma once

#include "Entity.h"
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "View.h"

#include <string>
#include <vector>
#include <memory>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h> 
#include <assimp/postprocess.h>
#include <assimp/mesh.h>

using std::string;
using std::vector;
using std::shared_ptr;
using std::map;

#include "tiny_gltf.h"

using namespace tinygltf;

namespace RenderLab
{
	class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

    shared_ptr<Entity>  loadAssimpModel(string filename);
    shared_ptr<Entity>  loadGLTFModel(string filename);

  private:
    void processNode(const aiScene* scene, shared_ptr<Entity> parent, aiNode* node);
    void populateMeshMaterial(const aiScene* scene, shared_ptr<Mesh> rlMesh, shared_ptr<Material> rlMaterial, aiMesh* mesh);
    shared_ptr<Texture> loadTexture(const char* filename);

    void printLog(string s);
    mat4 getTransform(const Node &node);
    shared_ptr<Entity> processNode(Model &model, const Node &node);
    shared_ptr<Mesh> processMesh(Model &model, const Primitive &primitive);

    map<string, shared_ptr<Texture>>          m_textureMap;
	};
}

