#include <domainmodel/cli/Generator.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace domainmodel::cli {
namespace {

using namespace domainmodel::ast;

[[nodiscard]] std::string upper_first(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  std::string value(text);
  value.front() = static_cast<char>(
      std::toupper(static_cast<unsigned char>(value.front())));
  return value;
}

[[nodiscard]] std::string package_path_from_file_path(std::string_view filePath) {
  std::string packagePath(filePath);
  std::ranges::replace(packagePath, '/', '.');
  while (!packagePath.empty() && packagePath.front() == '.') {
    packagePath.erase(packagePath.begin());
  }
  return packagePath;
}

[[nodiscard]] std::string resolved_type_name(const Type &type) {
  if (const auto *entity = dynamic_cast<const Entity *>(&type)) {
    return entity->name;
  }
  if (const auto *dataType = dynamic_cast<const DataType *>(&type)) {
    return dataType->name;
  }
  throw std::runtime_error("Unsupported domainmodel type.");
}

[[nodiscard]] std::string referenced_type_name(const Feature &feature) {
  if (!feature.type.getRefText().empty()) {
    return feature.type.getRefText();
  }
  if (feature.type.get()) {
    return resolved_type_name(*feature.type.get());
  }
  throw std::runtime_error("Cannot generate Java for unresolved feature type.");
}

[[nodiscard]] std::string super_type_name(const Entity &entity) {
  if (!entity.superType.has_value()) {
    return {};
  }
  if (!entity.superType->getRefText().empty()) {
    return entity.superType->getRefText();
  }
  if (entity.superType->get()) {
    return entity.superType->get()->name;
  }
  throw std::runtime_error("Cannot generate Java for unresolved super type.");
}

struct FeatureData {
  std::string field;
  std::string setterAndGetter;
};

[[nodiscard]] FeatureData generate_feature(const Feature &feature) {
  auto type = referenced_type_name(feature);
  if (feature.many) {
    type += "[]";
  }
  const auto upperName = upper_first(feature.name);
  return {
      .field = "    private " + type + " " + feature.name + ";",
      .setterAndGetter =
          "    public void set" + upperName + "(" + type + " " + feature.name +
          ") {\n"
          "        this." + feature.name + " = " + feature.name + ";\n"
          "    }\n\n"
          "    public " + type + " get" + upperName + "() {\n"
          "        return " + feature.name + ";\n"
          "    }\n",
  };
}

[[nodiscard]] std::string generate_entity(const Entity &entity) {
  std::vector<FeatureData> featureData;
  featureData.reserve(entity.features.size());
  for (const auto &feature : entity.features) {
    if (feature) {
      featureData.push_back(generate_feature(*feature));
    }
  }

  std::string out;
  out += "class " + entity.name;
  if (const auto maybeExtends = super_type_name(entity); !maybeExtends.empty()) {
    out += " extends " + maybeExtends;
  }
  out += " {\n";

  for (const auto &feature : featureData) {
    out += feature.field + "\n";
  }
  if (!featureData.empty()) {
    out += "\n";
  }

  for (std::size_t index = 0; index < featureData.size(); ++index) {
    out += featureData[index].setterAndGetter;
    if (index + 1 < featureData.size()) {
      out += "\n";
    }
  }

  out += "}\n";
  return out;
}

[[nodiscard]] std::string generate_entity_file_content(
    const Entity &entity, std::string_view packagePath) {
  std::string out;
  out += "package " + std::string(packagePath) + ";\n\n";
  out += generate_entity(entity);
  return out;
}

void generate_abstract_elements(const std::filesystem::path &destination,
                                const std::vector<std::unique_ptr<AbstractElement>> &elements,
                                std::string filePath,
                                std::string &lastGeneratedPath) {
  const auto fullPath = destination / filePath;
  std::filesystem::create_directories(fullPath);
  lastGeneratedPath = fullPath.string();

  const auto packagePath = package_path_from_file_path(filePath);
  for (const auto &element : elements) {
    if (const auto *package = dynamic_cast<const PackageDeclaration *>(element.get())) {
      auto packagePath = package->name;
      std::ranges::replace(packagePath, '.', '/');
      generate_abstract_elements(destination, package->elements,
                                 (std::filesystem::path(filePath) /
                                  std::filesystem::path(packagePath))
                                     .string(),
                                 lastGeneratedPath);
    } else if (const auto *entity = dynamic_cast<const Entity *>(element.get())) {
      const auto filePathValue = fullPath / (entity->name + ".java");
      std::ofstream out(filePathValue, std::ios::binary);
      if (!out.is_open()) {
        throw std::runtime_error("Unable to open output file: " +
                                 filePathValue.string());
      }
      out << generate_entity_file_content(*entity, packagePath);
    }
  }
}

} // namespace

std::string generate_java(const ast::DomainModel &model, std::string_view filePath,
                          std::optional<std::string_view> destination) {
  const auto data = extract_destination_and_name(filePath, destination);
  std::filesystem::create_directories(data.destination);

  std::string lastGeneratedPath;
  generate_abstract_elements(data.destination, model.elements, data.name,
                             lastGeneratedPath);
  return (std::filesystem::path(data.destination) / data.name).string();
}

} // namespace domainmodel::cli
