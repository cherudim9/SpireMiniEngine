#include "CoreLib/Basic.h"
#include "CoreLib/Tokenizer.h"
#include "CoreLib/LibIO.h"
#include "CoreLib/VectorMath.h"
#include <Windows.h>
#include <iostream>
#include <vector>

using namespace CoreLib::Basic;
using namespace CoreLib::IO;
using namespace CoreLib::Text;
using namespace VectorMath;

String ExtractField(const String & src, const String & fieldName)
{
	int idx = src.IndexOf(fieldName);
	if (idx != -1)
	{
		int endIdx = src.IndexOf(L' ', idx);
		int endIdx2 = Math::Min(src.IndexOf(L'\n', idx), src.IndexOf(L'\r', idx));

		if (endIdx == -1 || endIdx2 != -1 && endIdx2 < endIdx)
			endIdx = endIdx2;
		return src.SubString(idx + fieldName.Length(), endIdx - idx - fieldName.Length());
	}
	return "";
}

Vec3 ParseRotation(String src)
{
	Vec3 rs;
	TokenReader p(src);
	p.Read("(");
	p.Read("Pitch");
	p.Read("=");
	rs.x = (float)p.ReadDouble();
	p.Read(",");
	p.Read("Yaw");
	p.Read("=");
	rs.y = (float)p.ReadDouble();
	p.Read(",");
	p.Read("Roll");
	p.Read("=");
	rs.z = (float)p.ReadDouble();
	return rs;
}

Vec3 ParseTranslation(String src)
{
	Vec3 rs;
	TokenReader p(src);
	p.Read("(");
	p.Read("X");
	p.Read("=");
	rs.x = (float)p.ReadDouble();
	p.Read(",");
	p.Read("Y");
	p.Read("=");
	rs.y = (float)p.ReadDouble();
	p.Read(",");
	p.Read("Z");
	p.Read("=");
	rs.z = (float)p.ReadDouble();
	return rs;
}

String IndentString(String src)
{
	StringBuilder  sb;
	int indent = 0;
	bool beginTrim = true;
	for (int c = 0; c < src.Length(); c++)
	{
		auto ch = src[c];
		if (ch == '\n')
		{
			sb << "\n";

			beginTrim = true;
		}
		else
		{
			if (beginTrim)
			{
				while (c < src.Length() - 1 && (src[c] == '\t' || src[c] == '\n' || src[c] == '\r' || src[c] == ' '))
				{
					c++;
					ch = src[c];
				}
				for (int i = 0; i < indent - 1; i++)
					sb << '\t';
				if (ch != '}' && indent > 0)
					sb << '\t';
				beginTrim = false;
			}

			if (ch == L'{')
				indent++;
			else if (ch == L'}')
				indent--;
			if (indent < 0)
				indent = 0;

			sb << ch;
		}
	}
	return sb.ProduceString();
}

int wmain(int argc, const wchar_t ** argv)
{
	String FBXFolder = R"(C:\Users\cherudim\Documents\Game\FactoryDistrict\FBX)";
	String MeshFolder = R"(C:\Users\cherudim\Documents\Game\FactoryDistrict\Mesh)";
	String ModelImporter = R"(C:\Users\cherudim\Source\Repos\SpireMiniEngine\x64\Release\ModelImporter.exe)";
	if (argc <= 1)
		return 0;
	String fileName = String::FromWString(argv[1]);
	String src = File::ReadAllText(fileName);
	TokenReader parser(src);
	StringBuilder sb;
	sb << String(R"(
Camera
{
    name "FreeCam"
    position [0.0 120.0 220.0]
    orientation [0.0 0.1 0.0] // yaw, pitch, roll
    znear 10.0
    zfar 50000.0
    fov 60.0
}

FreeRoamCameraController
{
    name "CamControl"
    TargetCamera "FreeCam"
    Speed 2000.0
}

Atmosphere
{
    SunDir [-0.5 0.8 0.4]
    AtmosphericFogScaleFactor 0.5 
}

DirectionalLight
{
    name "sunlight"
    transform [1 0 0 0   0 1 0 0    0 0 1 0    0 5000 0 1]
    Direction [-0.5 0.8 0.4]
    Color [1.2 1.2 1.2]
    EnableCascadedShadows true
    NumShadowCascades 4
    ShadowDistance 1500
    TransitionFactor 0.8 
}
)");
	while (!parser.IsEnd())
	{
		if (parser.ReadToken().Content == "Begin" &&
			parser.ReadToken().Content == "Actor" && 
			parser.ReadToken().Content == "Class" &&
			parser.ReadToken().Content == "=")
		{
			auto beginPos = parser.NextToken().Position;
			if (parser.ReadToken().Content == "StaticMeshActor")
			{
				while (!(parser.NextToken().Content == "End" && parser.NextToken(1).Content == "Actor"))
				{
					parser.ReadToken();
				}
				auto endToken = parser.ReadToken();
				auto endPos = endToken.Position;
				auto actorStr = src.SubString(beginPos.Pos, endPos.Pos - beginPos.Pos);
				auto name = ExtractField(actorStr, "Name=");
				auto mesh = ExtractField(actorStr, "StaticMesh=");
				auto location = ExtractField(actorStr, "RelativeLocation=");
				auto rotation = ExtractField(actorStr, "RelativeRotation=");
				auto scale = ExtractField(actorStr, "RelativeScale3D=");
				auto material = ExtractField(actorStr, "OverrideMaterials(0)=");
				Matrix4 transform;
				Matrix4::CreateIdentityMatrix(transform);
				if (scale.Length())
				{
					Matrix4 matS;
					auto s = ParseTranslation(scale);
					Matrix4::Scale(matS, s.x, s.z, s.y);
					Matrix4::Multiply(transform, matS, transform);
				}
				if (rotation.Length())
				{
					Matrix4 rot;
					auto r = ParseRotation(rotation);
					double pi = acos(-1.0);
					r.x *= pi / 180;
					r.y *= pi / 180;
					r.z *= pi / 180;
					Matrix4::Rotation(rot, -r.y, r.z, r.x);
					Matrix4::Multiply(transform, rot, transform);
				}
				if (location.Length())
				{
					Matrix4 matTrans;
					auto s = ParseTranslation(location);
					Matrix4::Translation(matTrans, s.x, s.z, s.y);
					Matrix4::Multiply(transform, matTrans, transform);
				}

				std::vector<String> Meshes;
				String FBXName = FBXFolder + R"(\)" + mesh.SubString(mesh.IndexOf('.') + 1, mesh.Length() - mesh.IndexOf('.') - 3) + ".FBX";
				String MeshPrefix = MeshFolder + R"(\)" + mesh.SubString(mesh.IndexOf('.') + 1, mesh.Length() - mesh.IndexOf('.') - 3);
				int ans = system((ModelImporter + " " + FBXName + " " + MeshPrefix + ".mesh").Buffer());

				if (INVALID_FILE_ATTRIBUTES == GetFileAttributes((MeshPrefix + R"(.mesh)").ToWString()) && GetLastError() == ERROR_FILE_NOT_FOUND)
				{
					//number of meshes > 1
					int num = 0;
					do
					{
						Meshes.push_back(MeshPrefix + "(" + num + ").mesh");
						num++;
					} while (INVALID_FILE_ATTRIBUTES != GetFileAttributes((MeshPrefix + "(" + num + ")" + ".mesh").ToWString()) || GetLastError() != ERROR_FILE_NOT_FOUND);
				}

				else
				{
					Meshes.push_back(MeshPrefix + ".mesh");
				}

				int id = 0;
				for (auto mesh_filename : Meshes)
				{
					sb << "StaticMesh\n{\n";
					sb << "name \"" << name << "(" << id << ")\"\n";
					id++;
					sb << "mesh \"" << Path::GetFileName(mesh_filename) << "\"\n";

					sb << "transform [";
					for (int i = 0; i < 16; i++)
						sb << transform.values[i] << " ";
					sb << "]\n";

					if (material.Length() && material != "None\n" && 0)
					{
						sb << "material \"" << material.SubString(material.IndexOf('.') + 1, material.Length() - material.IndexOf('.') - 3) << ".material\"\n";
					}
					else
					{
						sb << "material { shader \"DefaultPattern.shader\"}\n";
					}

					sb << "}\n";
				}
			}
		}
	}
	File::WriteAllText(Path::ReplaceExt(fileName, "level"), IndentString(sb.ProduceString()));
    return 0;
}

