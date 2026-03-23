#pragma once

#include <stdexcept>
#include <string>

namespace pegium::utils {

/// Base exception type used for Pegium-specific failures.
class PegiumError : public std::runtime_error {
public:
  explicit PegiumError(const std::string &message);
};

class CacheDisposedError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class CliError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class CliUsageError : public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;
};

class DocumentBuilderError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class DocumentFactoryError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class DocumentStoreError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class FileSystemError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class LanguageServerError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class MissingAstDocumentError : public std::logic_error {
public:
  using std::logic_error::logic_error;
};

class ServiceRegistrationError : public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;
};

class ServiceRegistryError : public PegiumError {
public:
  using PegiumError::PegiumError;
};

class ValidationRegistryError : public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;
};

} // namespace pegium::utils
