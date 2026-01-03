/*
 * Part of Spaghetti.
 * Copyright 2025 Dario Mambro.
 * Distriuted under the GNU Affero General Public License.
 */

#include "app.h"
#include <cstdint>
#include <map>
#include <memory>
#include <set>

using ProcessorId = uint64_t;
using LinkId = uint64_t;
constexpr inline ProcessorId UNLINKED = 0;

using TextureRef = std::shared_ptr<wgpu::raii::Texture>;
using BufferRef = std::shared_ptr<wgpu::raii::Buffer>;

enum class Type {
  value,
  image,
  buffer,
  curve,
  text,
};

enum class Encoding { floating, sinteger, uinteger };

struct DataSignature {
  Type type = Type::value;
  Encoding encoding = Encoding::floating;
  uint32_t num_coords = 1; // all images have 4
  uint32_t array_length = 1;

  bool operator==(DataSignature const&) const = default;
};

bool CanLink(DataSignature const& output, DataSignature const& input);

struct Data {
  std::string name;
  DataSignature signature;

  static std::unique_ptr<Data> Make(DataSignature signature);
  std::unique_ptr<Data> ConvertTo(DataSignature inputSignature) const;

  virtual ~Data() = default;
};

template<class ValueDataClass>
struct TData : Data {
  using ValueType = ValueDataClass;
  std::vector<ValueType> data;
};

struct Image : TData<TextureRef> {};

struct Buffer : TData<BufferRef> {};

struct Text : TData<std::string> {};

template<class ElementTypeClass>
struct VecData : TData<std::vector<typename ElementTypeClass>> {

  using ElementType = ElementTypeClass;

  void ResetValueTo(ElementType valueToResetTo) {
    this->data.resize(this->signature.array_length, typename TData<std::vector<ElementType>>::ValueType{});
    for (auto& e : this->data) {
      e.resize(this->signature.num_coords, valueToResetTo);
    }
  }
};

struct Floating : VecData<float> {};
struct SInteger : VecData<int32_t> {};
struct UInteger : VecData<uint32_t> {};

struct CurvePoint {
  std::array<float, 2> position;
  std::array<float, 2> tangent_left;
  std::array<float, 2> tangent_right;
};
struct CurvePoints {
  std::vector<CurvePoint> points;
};

struct Curve : VecData<CurvePoints> {};

struct DataAddress {
  ProcessorId processor = UNLINKED;
  uint32_t data_index = 0;
};

struct Input {
  std::string name;
  DataSignature signature;
  DataAddress linkedOutput;
  std::unique_ptr<Data> default_value{};
  std::unique_ptr<Data> convertedData{};

  Data* GetInputData() const;
  void ResetDefaultValue();
  bool SetupLink();
};

enum class ProcessorType { fragment_shader, compute_shader, image_reader, buffer_reader, script, builtin, group };

class Processor {
public:
  ProcessorId const id;
  std::string display_name;
  std::string template_name;

  static Processor* Get(ProcessorId id);
  virtual ~Processor();

  Processor(const Processor&) = delete;
  Processor(Processor&&) = delete;
  Processor& operator=(const Processor&) = delete;
  Processor& operator=(Processor&&) = delete;

  template<class ProcessorClass>
  static Processor* Make() {
    ++count;
    processors[count] = std::make_unique<ProcessorClass>();
    processors[count].get();
  }

  template<class ProcessorClass>
  static Processor* MakeSwap(ProcessorId id) {
    if (id >= count) {
      return nullptr;
    }
    processors[id] = std::make_unique<ProcessorClass>(id);
    processors[id].get();
  }

  virtual void Process() {}
  virtual void OnInputChanged() {}
  virtual void OnOutputChanged() {}
  virtual bool CanProcess() const;

  void AddInput(Input in);
  void AddOutput(std::unique_ptr<Data> out);
  void RemoveInput(uint32_t index);
  void RemoveOutput(uint32_t index);
  void MoveInput(uint32_t prev_index, uint32_t new_index);
  void MoveOuput(uint32_t prev_index, uint32_t new_index);
  void SetInput(uint32_t index, Input in);
  void SetOutput(uint32_t index, std::unique_ptr<Data> out);
  void AddInputLink(uint32_t input_index, DataAddress linkedOutput);
  void AddOutputLink(uint32_t output_index, DataAddress linkedInput);
  bool NeedsUpdate();
  void SetNeedsUpdate();
  bool HasLinkedInputs();

  std::vector<std::unique_ptr<Data>> const& GetOutputs() const { return outputs; }
  std::vector<Input> const& GetInputs() const { return inputs; }
  std::map<uint32_t, std::vector<DataAddress>> const& GetOutputLinks() const { return outputLinks; }

protected:
  Processor();
  Processor(ProcessorId id);

  std::vector<Input> inputs;
  std::vector<std::unique_ptr<Data>> outputs;
  std::map<uint32_t, std::vector<DataAddress>> outputLinks;

private:
  static inline ProcessorId count = 0;
  static inline std::map<ProcessorId, std::unique_ptr<Processor>> processors;
  bool needs_update{ true };
};

class PixelProcessor : public Processor {
public:
  PixelProcessor();
  void Process() override;
};

class ComputeProcessor : public Processor {
public:
  ComputeProcessor();
  void Process() override;
};

class ImageReader : public Processor {
public:
  ImageReader();
  void Process() override;
};

class ScriptProcessor : public Processor {
public:
  ScriptProcessor();
  void Process() override;
};

using BuiltinProcessingCall = std::function<void(std::vector<Input> const&, std::vector<std::unique_ptr<Data>>&)>;

class BuiltinProcessor : public Processor {
public:
  BuiltinProcessor();
  void SetProcessingCall(BuiltinProcessingCall call);
  void Process() override;

private:
  BuiltinProcessingCall process_call{};
};

class Graph {
public:
  void Execute();
  LinkId CreateLink(DataAddress output, DataAddress input);
  void RemoveLink(LinkId link_id);

private:
  struct LinkData {
    DataAddress output;
    DataAddress input;
  };
  std::map<LinkId, LinkData> links;
  std::vector<ProcessorId> processors;
  std::set<ProcessorId> no_input_processors;
  LinkId linkCount = 0;
};

class GroupProcessor : public Processor {
public:
  GroupProcessor();
  void Process() override { graph.Execute(); }

private:
  Graph graph;
};
