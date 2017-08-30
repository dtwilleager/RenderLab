#include "stdafx.h"
#include "ModelLoader.h"

#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>

#include <IL\il.h>
#include <atlstr.h>

using std::make_shared;
using std::static_pointer_cast;

using glm::mat4;
using glm::vec3;
using glm::dquat;

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace RenderLab
{
  ModelLoader::ModelLoader()
  {
    ilInit();
  }


  ModelLoader::~ModelLoader()
  {
  }

  mat4 ModelLoader::getTransform(const Node &node)
  {
    mat4 transform;
    if (node.matrix.size() == 16) {
      // Use `matrix' attribute
      transform = glm::make_mat4(node.matrix.data());
    }
    else {
      mat4 scale, rotation, translation;
      // Assume Trans x Rotate x Scale order
      if (node.scale.size() == 3) {
        scale = glm::scale(mat4(), vec3(node.scale[0], node.scale[1], node.scale[2]));
      }

      if (node.rotation.size() == 4) {
        rotation = glm::rotate(mat4(), (float)node.rotation[0], vec3(node.rotation[1], node.rotation[2], node.rotation[3]));
      }

      if (node.translation.size() == 3) {
        translation = glm::translate(mat4(), vec3(node.translation[0], node.translation[1], node.translation[2]));
      }

      transform = translation * rotation * scale;
    }

    return transform;
  }

  shared_ptr<Entity> ModelLoader::processNode(Model &model, const Node &node)
  {
    shared_ptr<Entity> entity = make_shared<Entity>(node.name);
    mat4 transform = getTransform(node);
    entity->setTransform(transform);

    if (node.mesh != -1)
    {
      shared_ptr<RenderComponent> renderComponent = make_shared<RenderComponent>(node.name);
      tinygltf::Mesh mesh = model.meshes[node.mesh];
      for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        if (mesh.primitives[i].indices > 0)
        {
          renderComponent->addMesh(processMesh(model, mesh.primitives[i]));
        }
      }
      entity->addComponent(renderComponent);
    }

    for (size_t i = 0; i < node.children.size(); ++i) {
      entity->addChild(processNode(model, model.nodes[node.children[i]]));
    }
    return entity;
  }

  shared_ptr<Mesh> ModelLoader::processMesh(Model &model, const Primitive &primitive)
  {
    size_t numAttributes = primitive.attributes.size();
    size_t numVerts = 0;
    int bufferIndex = -1;
    shared_ptr<Mesh> mesh = nullptr; // make_shared<Mesh>("", numVerts, numAttributes);

    map<string, int>::const_iterator it(primitive.attributes.begin());
    map<string, int>::const_iterator itEnd(primitive.attributes.end());

    for (; it != itEnd; it++) {
      const Accessor &accessor = model.accessors[it->second];
      numVerts = accessor.count;
      int size = 1;
      switch (accessor.type) 
      {
      case TINYGLTF_TYPE_SCALAR:
        size = 1;
        break;
      case TINYGLTF_TYPE_VEC2:
        size = 2;
        break;
      case TINYGLTF_TYPE_VEC3:
        size = 3;
        break;
      case TINYGLTF_TYPE_VEC4:
        size = 4;
        break;
      }

      if (it->first.compare("POSITION"))
      {
        bufferIndex = 0;
      }
      else if (it->first.compare("NORMAL"))
      {
        bufferIndex = 1;
      }
      else if (it->first.compare("TEXCOORD_0"))
      {
        bufferIndex = 2;
      }

      if (mesh == nullptr)
      {
        mesh = make_shared<Mesh>("", numVerts, numAttributes);
      }
      size_t numBytes = size*numVerts * sizeof(float);
      float* data = (float*)malloc(numBytes);
      const unsigned char* buffer = model.buffers[model.bufferViews[accessor.bufferView].buffer].data.data();
      size_t stride = model.bufferViews[accessor.bufferView].byteStride;
      int index = 0;
      for (int i = 0; i < numVerts; ++i)
      {
        float* srcData = (float*)&(buffer[accessor.byteOffset + (i*stride)]);
        for (int j = 0; j < size; j++)
        {
          data[index++] = srcData[j];
        }
      }
      mesh->addVertexBuffer(bufferIndex, size, numBytes, data);
      free(data);
    }

    Accessor &indexAccessor = model.accessors[primitive.indices];
    size_t numBytes = indexAccessor.count * sizeof(unsigned int);
    unsigned int* indexData = (unsigned int*)malloc(numBytes);
    const unsigned char* buffer = model.buffers[model.bufferViews[indexAccessor.bufferView].buffer].data.data();
    unsigned int* srcData = (unsigned int*)&(buffer[indexAccessor.byteOffset]);

    for (int i = 0; i < indexAccessor.count; ++i)
    {
      indexData[i++] = srcData[i];
    }
    mesh->addIndexBuffer(indexAccessor.count, indexData);
    free(indexData);

    vec4 baseColor(1.0f, 1.0f, 1.0f, 1.0f);
    float metallic = 1.0f;
    float roughness = 1.0f;

    tinygltf::Material material = model.materials[primitive.material];
    if (material.values.find("baseColorFactor") != material.values.end())
    {
      vector<double> baseColorFactor = material.values["baseColorFactor"].number_array;
      baseColor[0] = (float)baseColorFactor[0];
      baseColor[1] = (float)baseColorFactor[1];
      baseColor[2] = (float)baseColorFactor[2];
      baseColor[3] = (float)baseColorFactor[3];
    }
    //if (material.values.find("baseColorTexture") != material.values.end())
    //{

    //}
    if (material.values.find("metallicFactor") != material.values.end())
    {
      vector<double> metallicFactor = material.values["metallicFactor"].number_array;
      metallic = (float)metallicFactor[0];
    }
    if (material.values.find("roughnessFactor") != material.values.end())
    {
      vector<double> roughnessFactor = material.values["roughnessFactor"].number_array;
      roughness = (float)roughnessFactor[0];
    }
    //if (material.values.find("metallicRoughnessTexture") != material.values.end())
    //{

    //}
    shared_ptr<Material>rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
    rlMaterial->setAlbedoColor(baseColor);
    rlMaterial->setMetallic(metallic);
    rlMaterial->setRoughness(roughness);
    mesh->setMaterial(rlMaterial);

    return mesh;
  }

  shared_ptr<Entity> ModelLoader::loadGLTFModel(string filename)
  {
    Model model;
    TinyGLTF loader;
    std::string err;
    shared_ptr<Entity> rootEntity = make_shared<Entity>("Scene Root");

    loader.LoadASCIIFromFile(&model, &err, filename);

    if (!err.empty()) {
      printf("Err: %s\n", err.c_str());
    }

    const Scene &scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); i++) {
      rootEntity->addChild(processNode(model, model.nodes[scene.nodes[i]]));
    }

    return rootEntity;
  }



  shared_ptr<Entity> ModelLoader::loadAssimpModel(string filename)
  {
    Assimp::Importer importer;
    shared_ptr<Entity> rootEntity = NULL;
    shared_ptr<Mesh> rlMesh;
    shared_ptr<Material> rlMaterial;
    shared_ptr<RenderComponent> renderComponent;

    const aiScene* scene = importer.ReadFile(filename.c_str(),
      aiProcess_CalcTangentSpace |
      aiProcess_Triangulate |
      aiProcess_JoinIdenticalVertices |
      aiProcess_SortByPType |
      aiProcess_GenSmoothNormals);

    // If the import failed, report it
    if (!scene)
    {
      const char* error = importer.GetErrorString();
      return NULL;
    }

    unsigned int numMeshes = scene->mNumMeshes;
    unsigned int numMaterials = scene->mNumMaterials;

    rootEntity = make_shared<Entity>(scene->mRootNode->mName.C_Str());
    if (scene->mRootNode->mNumMeshes > 0)
    {
      // Create Entity for this node
      renderComponent = make_shared<RenderComponent>(scene->mRootNode->mName.C_Str());

      for (unsigned int i = 0; i<scene->mRootNode->mNumMeshes; i++)
      {
        aiMesh* mesh = scene->mMeshes[scene->mRootNode->mMeshes[i]];
        unsigned int numVerts = mesh->mNumVertices;
        unsigned int numBuffers = 1;

        if (mesh->HasNormals())
        {
          numBuffers++;
        }

        if (mesh->HasTextureCoords(0))
        {
          numBuffers++;
        }

        if (mesh->HasTangentsAndBitangents())
        {
          numBuffers += 2;
        }

        rlMesh = make_shared<Mesh>(scene->mRootNode->mName.C_Str(), numVerts, numBuffers);
        int texIndex = 0;
        aiString texturePath;
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        if (material->GetTexture(aiTextureType_DIFFUSE, texIndex, &texturePath) == AI_SUCCESS)
        {
          int normalIndex = 0;
          aiString normalPath;
          if (material->GetTexture(aiTextureType_HEIGHT, normalIndex, &normalPath) == AI_SUCCESS && texturePath != normalPath)
          {
            rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
          }
          else
          {
            rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
          }
        }
        else
        {
          rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
          if (material->GetTexture(aiTextureType_HEIGHT, texIndex, &texturePath) == AI_SUCCESS)
          {
            printLog("UNLIT NORMAL MAP");
          }
        }
        populateMeshMaterial(scene, rlMesh, rlMaterial, mesh);
        rlMesh->setMaterial(rlMaterial);
        renderComponent->addMesh(rlMesh);
      }
      rootEntity->addComponent(renderComponent);
    }
    mat4 transform = glm::make_mat4((float*)&(scene->mRootNode->mTransformation));
    rootEntity->setTransform(transform);

    // Process the children
    for (unsigned int i = 0; i<scene->mRootNode->mNumChildren; i++)
    {
      processNode(scene, rootEntity, scene->mRootNode->mChildren[i]);
    }

    printLog("Done Loaded Mesh");

    return (rootEntity);
  }

  void ModelLoader::processNode(const aiScene* scene, shared_ptr<Entity> parent, aiNode* node)
  {
    shared_ptr<Entity> entity = NULL;
    shared_ptr<Mesh> rlMesh;
    shared_ptr<Material> rlMaterial;
    shared_ptr<RenderComponent> renderComponent;

    entity = make_shared<Entity>(node->mName.C_Str());
    if (node->mNumMeshes > 0)
    {
      renderComponent = make_shared<RenderComponent>(node->mName.C_Str());

      for (unsigned int i = 0; i<node->mNumMeshes; i++)
      {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        unsigned int numVerts = mesh->mNumVertices;
        unsigned int numBuffers = 1;

        if (mesh->HasNormals())
        {
          numBuffers++;
        }

        if (mesh->HasTextureCoords(0))
        {
          numBuffers++;
        }

        if (mesh->HasTangentsAndBitangents())
        {
          numBuffers += 2;
        }

        rlMesh = make_shared<Mesh>(node->mName.C_Str(), numVerts, numBuffers);
        printLog("Loaded Mesh: " + std::to_string(numVerts));
        int texIndex = 0;
        aiString texturePath;
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        if (material->GetTexture(aiTextureType_DIFFUSE, texIndex, &texturePath) == AI_SUCCESS)
        {
          int normalIndex = 0;
          aiString normalPath;
          if (material->GetTexture(aiTextureType_HEIGHT, normalIndex, &normalPath) == AI_SUCCESS && texturePath != normalPath)
          {
            rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
          }
          else
          {
            rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
          }
        }
        else
        {
          rlMaterial = make_shared<Material>("ModelMaterial", Material::DEFERRED_LIT);
          if (material->GetTexture(aiTextureType_HEIGHT, texIndex, &texturePath) == AI_SUCCESS)
          {
            printLog("UNLIT NORMAL MAP");
          }
        }

        populateMeshMaterial(scene, rlMesh, rlMaterial, mesh);
        rlMesh->setMaterial(rlMaterial);
        renderComponent->addMesh(rlMesh);
      }
      entity->addComponent(renderComponent);
    }
    mat4 transform = glm::make_mat4((float*)&(node->mTransformation));
    entity->setTransform(transform);
    parent->addChild(entity);

    // Process the children
    for (unsigned int i = 0; i<node->mNumChildren; i++)
    {
      processNode(scene, entity, node->mChildren[i]);
    }
  }

  void ModelLoader::populateMeshMaterial(const aiScene* scene, shared_ptr<Mesh> rlMesh, shared_ptr<Material> rlMaterial, aiMesh* mesh)
  {
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    unsigned int numFaces = mesh->mNumFaces;
    unsigned int numVerts = mesh->mNumVertices;
    unsigned int bufferIndex = 0;

    float* verts = new float[numVerts * 3];
    unsigned int vindex = 0;
    for (unsigned int i = 0; i<numVerts; i++)
    {
      verts[vindex++] = mesh->mVertices[i].x;
      verts[vindex++] = -mesh->mVertices[i].y;
      verts[vindex++] = mesh->mVertices[i].z;
    }
    rlMesh->addVertexBuffer(bufferIndex++, 3, sizeof(float)*numVerts * 3, verts);

    if (mesh->HasNormals())
    {
      float* normals = new float[numVerts * 3];
      unsigned int nindex = 0;
      for (unsigned int i = 0; i<numVerts; i++)
      {
        normals[nindex++] = mesh->mNormals[i].x;
        normals[nindex++] = -mesh->mNormals[i].y;
        normals[nindex++] = mesh->mNormals[i].z;
      }
      rlMesh->addVertexBuffer(bufferIndex++, 3, sizeof(float)*numVerts * 3, normals);
    }

    if (mesh->HasTextureCoords(0))
    {
      float* texCoords = new float[numVerts * 2];
      unsigned int tindex = 0;
      for (unsigned int i = 0; i<numVerts; i++)
      {
        texCoords[tindex++] = mesh->mTextureCoords[0][i].x;
        texCoords[tindex++] = mesh->mTextureCoords[0][i].y;
      }
      rlMesh->addVertexBuffer(bufferIndex++, 2, sizeof(float)*numVerts * 2, texCoords);
    }

    if (mesh->HasTangentsAndBitangents())
    {
      float* tangents = new float[numVerts * 3];
      float* bitangents = new float[numVerts * 3];
      unsigned int tbindex = 0;
      for (unsigned int i = 0; i<numVerts; i++)
      {
        tangents[tbindex] = mesh->mTangents[i].x;
        bitangents[tbindex++] = mesh->mBitangents[i].x;
        tangents[tbindex] = -mesh->mTangents[i].y;
        bitangents[tbindex++] = -mesh->mBitangents[i].y;
        tangents[tbindex] = mesh->mTangents[i].z;
        bitangents[tbindex++] = mesh->mBitangents[i].z;
      }
      rlMesh->addVertexBuffer(bufferIndex++, 3, sizeof(float)*numVerts * 3, tangents);
      rlMesh->addVertexBuffer(bufferIndex++, 3, sizeof(float)*numVerts * 3, bitangents);
    }

    unsigned int* indexBuffer = new unsigned int[numFaces * 3];
    unsigned int iindex = 0;
    for (unsigned int i = 0; i<numFaces; i++)
    {
      const struct aiFace* face = &mesh->mFaces[i];
      indexBuffer[iindex++] = face->mIndices[0];
      indexBuffer[iindex++] = face->mIndices[1];
      indexBuffer[iindex++] = face->mIndices[2];
    }
    rlMesh->addIndexBuffer(numFaces * 3, indexBuffer);

    for (unsigned int i = 0; i<material->mNumProperties;)
    {
      aiMaterialProperty* property = material->mProperties[i];
      i++;
    }

    aiString name;

    glm::vec3 color;
    glm::vec4 color4;
    aiColor3D aColor;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, aColor) == AI_SUCCESS)
    {
      color4.r = aColor.r;
      color4.g = aColor.g;
      color4.b = aColor.b;
      color4.a = 1.0f;
      rlMaterial->setAlbedoColor(color4);
    }

    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, aColor) == AI_SUCCESS)
    {
      color.r = aColor.r;
      color.g = aColor.g;
      color.b = aColor.b;
      rlMaterial->setEmissiveColor(color);
    }

    int twoSided;
    if (material->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
    {
      rlMaterial->setTwoSided((twoSided == 0 ? false : true));
    }

    int wireframe;
    if (material->Get(AI_MATKEY_ENABLE_WIREFRAME, wireframe) == AI_SUCCESS)
    {
      rlMaterial->setTwoSided((wireframe == 0 ? false : true));
    }

    int texIndex = 0;
    aiString texturePath;
    if (material->GetTexture(aiTextureType_DIFFUSE, texIndex, &texturePath) == AI_SUCCESS)
    {
      shared_ptr<Texture> texture = loadTexture((const char*)texturePath.C_Str());
      rlMaterial->setAlbedoTexture(texture);
      printLog("Loaded Texture: " + std::string(texturePath.C_Str()));
    }

    int normalIndex = 0;
    aiString normalPath;
    if (material->GetTexture(aiTextureType_HEIGHT, normalIndex, &normalPath) == AI_SUCCESS && texturePath != normalPath)
    {
      shared_ptr<Texture> normalTexture = loadTexture((const char*)normalPath.C_Str());
      rlMaterial->setNormalTexture(normalTexture);
      printLog("Loaded Normal Texture: " + std::string(normalPath.C_Str()));
    }

    //int metallicIndex = 0;
    //aiString metallicPath;
    //if (material->GetTexture(aiTextureType_AMBIENT, metallicIndex, &metallicPath) == AI_SUCCESS && texturePath != metallicPath)
    //{
    //  shared_ptr<Texture> metallicTexture = loadTexture((const char*)metallicPath.C_Str());
    //  rlMaterial->setMetallicTexture(metallicTexture);
    //  printLog("Loaded Metallic Texture: " + std::string(metallicPath.C_Str()));
    //}

    int roughnessIndex = 0;
    aiString roughnessPath;
    if (material->GetTexture(aiTextureType_SHININESS, roughnessIndex, &roughnessPath) == AI_SUCCESS && texturePath != roughnessPath)
    {
      shared_ptr<Texture> roughnessTexture = loadTexture((const char*)roughnessPath.C_Str());
      rlMaterial->setMetallicRoughnessTexture(roughnessTexture);
      printLog("Loaded Roughness Texture: " + std::string(roughnessPath.C_Str()));
    }
  }

  shared_ptr<Texture> ModelLoader::loadTexture(const char* filename)
  {
    ILuint ilDiffuseID;
    ILboolean success;
    shared_ptr<Texture> texture = nullptr;

    map<string, shared_ptr<Texture>>::iterator it = m_textureMap.find(filename);
    if (it != m_textureMap.end())
    {
      //element found;
      return it->second;
    }

    /* generate DevIL Image IDs */
    ilGenImages(1, &ilDiffuseID);
    ilBindImage(ilDiffuseID); /* Binding of DevIL image name */
    success = ilLoadImage((const wchar_t*)filename);
    //ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

    if (success) /* If no error occured: */
    {
      success = ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
      if (!success)
      {
        return NULL;
      }

      texture = make_shared<Texture>(filename, ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), ilGetInteger(IL_IMAGE_DEPTH),
        ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL), ilGetInteger(IL_IMAGE_SIZE_OF_DATA), ilGetInteger(IL_IMAGE_FORMAT));
      texture->setData(ilGetData());

      m_textureMap[filename] = texture;
    }

    return texture;
  }

  void ModelLoader::printLog(string s)
  {
    string st = s + "\n";
    TCHAR name[256];
    _tcscpy_s(name, CA2T(st.c_str()));
    OutputDebugString(name);
  }
}
