/*
 * Part of Spaghetti.
 * Copyright 2025 Dario Mambro.
 * Distriuted under the GNU Affero General Public License.
 */

#include "processor.h"
#include <algorithm>
#include <unordered_set>

Processor* Processor::Get(ProcessorId id) {
  auto it = processors.find(id);
  if (it != processors.end())
    return it->second.get();
  return nullptr;
}

Processor::~Processor() {}

bool Processor::CanProcess() const {
  for (auto const& in : inputs) {
    if (in.linkedOutput.processor == UNLINKED) {
      if (!in.default_value) {
        return false;
      }
    }
    if (in.GetInputData() == nullptr) {
      return false;
    }
  }
  return true;
}

void Processor::AddInput(Input in) {}

void Processor::AddOutput(std::unique_ptr<Data> out) {}

void Processor::RemoveInput(uint32_t index) {}

void Processor::RemoveOutput(uint32_t index) {}

void Processor::MoveInput(uint32_t prev_index, uint32_t new_index) {}

void Processor::MoveOuput(uint32_t prev_index, uint32_t new_index) {}

void Processor::SetInput(uint32_t index, Input in) {}

void Processor::SetOutput(uint32_t index, std::unique_ptr<Data> out) {}

void Processor::AddInputLink(uint32_t input_index, DataAddress linkedOutput) {
  if (input_index < inputs.size()) {
    inputs[input_index].linkedOutput = linkedOutput;
  }
  SetNeedsUpdate();
}

void Processor::AddOutputLink(uint32_t output_index, DataAddress linkedInput) {
  auto it = outputLinks.find(output_index);
  if (it != outputLinks.end()) {
    it->second.push_back(linkedInput);
  }
  else {
    auto links = std::vector<DataAddress>();
    links.push_back(linkedInput);
    outputLinks[output_index] = std::move(links);
  }
}

bool Processor::NeedsUpdate() {
  if (needs_update) {
    needs_update = false;
    return true;
  }
  return false;
}

void Processor::SetNeedsUpdate() {
  if (needs_update == false) {
    needs_update = true;
    for (auto& out_clients : outputLinks) {
      for (auto client : out_clients.second) {
        Processor::Get(client.processor)->SetNeedsUpdate();
      }
    }
  }
}

bool Processor::HasLinkedInputs() {
  bool anyInput = false;
  for (auto& in : inputs) {
    if (in.linkedOutput.processor != UNLINKED) {
      anyInput = true;
      break;
    }
  }
  return anyInput;
}

Processor::Processor()
  : id{ count } {}

Processor::Processor(ProcessorId id)
  : id{ id } {}

PixelProcessor::PixelProcessor() {}

void PixelProcessor::Process() {}

ComputeProcessor::ComputeProcessor() {}

void ComputeProcessor::Process() {}

ImageReader::ImageReader() {}

void ImageReader::Process() {}

ScriptProcessor::ScriptProcessor() {}

void ScriptProcessor::Process() {}

BuiltinProcessor::BuiltinProcessor() {}

void BuiltinProcessor::SetProcessingCall(BuiltinProcessingCall call) {
  process_call = call;
}

void BuiltinProcessor::Process() {
  if (process_call) {
    process_call(inputs, outputs);
  }
}

void Graph::Execute() {
  std::unordered_set<Processor*> done;
  std::unordered_set<Processor*> backlog;
  std::unordered_set<Processor*> ready;
  std::unordered_set<Processor*> processing;
  std::unordered_set<Processor*> prev_backlog;

  auto CheckReady = [&](Processor* p) {
    bool is_ready = true;
    for (auto& in : p->GetInputs()) {
      auto in_processor = in.linkedOutput.processor;
      if (!(in_processor == UNLINKED || done.find(Processor::Get(in_processor)) != done.end())) {
        is_ready = false;
        break;
      }
    }
    if (is_ready) {
      ready.insert(p);
    }
    else {
      backlog.insert(p);
    }
  };

  auto Process = [&](Processor* p) {
    if (p->NeedsUpdate()) {
      p->Process();
    }
    done.insert(p);
    auto const& outputs = p->GetOutputLinks();
    for (auto& out : outputs) {
      for (auto& client_data : out.second) {
        auto client = Processor::Get(client_data.processor);
        CheckReady(client);
      }
    }
  };

  for (auto pid : no_input_processors) {
    Process(Processor::Get(pid));
  }
  while (!backlog.empty() || !ready.empty()) {
    processing = std::move(ready);
    ready.clear();
    prev_backlog = std::move(backlog);
    backlog.clear();
    for (auto* p : processing) {
      Process(p);
    }
    for (auto* p : prev_backlog) {
      CheckReady(p);
    }
  }
}

LinkId Graph::CreateLink(DataAddress output, DataAddress input) {
  Processor::Get(input.processor)->AddInputLink(input.data_index, output);
  Processor::Get(output.processor)->AddOutputLink(output.data_index, input);
  no_input_processors.erase(input.processor);
  linkCount++;
  links[linkCount] = { output, input };
  return linkCount;
}

void Graph::RemoveLink(LinkId link_id) {
  auto it = links.find(link_id);
  if (it != links.end()) {
    auto in = it->second.input;
    auto in_processor = Processor::Get(in.processor);
    in_processor->RemoveInput(in.data_index);
    if (in_processor->HasLinkedInputs()) {
      no_input_processors.erase(in.processor);
    }
    else {
      no_input_processors.insert(in.processor);
    }
    auto out = it->second.output;
    Processor::Get(out.processor)->RemoveOutput(out.data_index);
  }
}

GroupProcessor::GroupProcessor() {}

Data* Input::GetInputData() const {
  if (auto p = Processor::Get(linkedOutput.processor)) {
    auto linkedData = p->GetOutputs()[linkedOutput.data_index].get();
    if (linkedData->signature == signature) {
      return linkedData;
    }
    else if (convertedData) {
      return convertedData.get();
    }
  }
  if (default_value) {
    return default_value.get();
  }
  return nullptr;
}

void Input::ResetDefaultValue() {
  default_value.reset();
  switch (signature.type) {
    case Type::value:
    case Type::curve:
    case Type::text: {
      default_value = Data::Make(signature);
    } break;
    case Type::image: {
      // todo link a default image
    } break;
    case Type::buffer: {
      // todo link a default buffer
    } break;
  }
  if (default_value) {
    default_value->name = name;
  }
}

bool Input::SetupLink() {
  convertedData.reset();
  if (auto p = Processor::Get(linkedOutput.processor)) {
    auto linkedData = p->GetOutputs()[linkedOutput.data_index].get();
    if (!CanLink(linkedData->signature, signature)) {
      return false;
    }
    if (linkedData->signature == signature) {
      return true;
    }
    convertedData = linkedData->ConvertTo(signature);
    return true;
  }
}

std::unique_ptr<Data> Data::Make(DataSignature signature) {
  switch (signature.type) {
    case Type::value: {
      switch (signature.encoding) {
        case Encoding::floating: {
          auto d = std::make_unique<Floating>();
          d->signature = signature;
          d->ResetValueTo(0);
          return std::move(d);
        } break;
        case Encoding::sinteger: {
          auto d = std::make_unique<SInteger>();
          d->signature = signature;
          d->ResetValueTo(0);
          return std::move(d);
        } break;
        case Encoding::uinteger: {
          auto d = std::make_unique<UInteger>();
          d->signature = signature;
          d->ResetValueTo(0);
          return std::move(d);
        } break;
      }
    } break;
    case Type::curve: {
      auto d = std::make_unique<Curve>();
      d->signature = signature;
      d->ResetValueTo({});
      for (auto& cc : d->data) {
        for (auto& c : cc) {
          c.points.resize(2);
          c.points[0] = { { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f } };
          c.points[1] = { { 1.f, 1.f }, { 1.f, 1.f }, { 1.f, 1.f } };
        }
      }
      return std::move(d);
    } break;
    case Type::image: {
      // allocate image
    } break;
    case Type::buffer: {
      // allocate buffer
    } break;
    case Type::text: {
      auto d = std::make_unique<Text>();
      d->data.resize(1, "");
      return std::move(d);
    } break;
  }

  return std::unique_ptr<Data>();
}

template<class DataClass>
void CopyData(Data* inData, Data const* outData) {
  auto in = static_cast<DataClass*>(inData);
  auto out = static_cast<DataClass const*>(outData);
  in->data = out->data;
}

template<class InDataClass = Floating, class OutDataClass = SInteger>
void CopyValueData(Data* inData, Data const* outData) {
  auto in = static_cast<InDataClass*>(inData);
  auto out = static_cast<OutDataClass const*>(outData);
  for (auto i = 0; i < inData->signature.array_length; ++i) {
    auto num_to_copy = std::min(inData->signature.num_coords, (uint32_t)out->data[i].size());
    if (num_to_copy == 1) {
      std::fill(in->data[i].begin(), in->data[i].end(), out->data[i][0]);
    }
    else {
      std::copy(&out->data[i][0], &out->data[i][num_to_copy], &in->data[i][0]);
      if constexpr (std::is_same_v<InDataClass, Curve>) {
        CurvePoints curve;
        curve.points.resize(2);
        curve.points[0] = { { 0.f, 0.f }, { 0.f, 0.f }, { 0.f, 0.f } };
        curve.points[0] = { { 0.f, 0.f }, { 0.f, 0.f }, { 0.f, 0.f } };
        std::fill(in->data[i].begin() + num_to_copy, in->data[i].end(), curve);
      }
      else {
        std::fill(in->data[i].begin() + num_to_copy, in->data[i].end(), 0);
      }
    }
  }
}

std::unique_ptr<Data> Data::ConvertTo(DataSignature inputSignature) const {
  if (!CanLink(signature, inputSignature))
    return nullptr;
  auto inData = Data::Make(inputSignature);

  if (signature.type == Type::value && inputSignature.type == Type::image) {
    // todo make image of 1pixel with the value
  }

  if (signature.type == Type::curve) {
    CopyValueData<Curve, Curve>(inData.get(), this);
  }
  if (signature.type == Type::value) {
    if (signature.encoding == Encoding::floating && inputSignature.encoding == Encoding::floating) {
      CopyValueData<Floating, Floating>(inData.get(), this);
    }
    if (signature.encoding == Encoding::floating && inputSignature.encoding == Encoding::sinteger) {
      CopyValueData<Floating, SInteger>(inData.get(), this);
    }
    if (signature.encoding == Encoding::floating && inputSignature.encoding == Encoding::uinteger) {
      CopyValueData<Floating, UInteger>(inData.get(), this);
    }
    if (signature.encoding == Encoding::sinteger && inputSignature.encoding == Encoding::floating) {
      CopyValueData<SInteger, Floating>(inData.get(), this);
    }
    if (signature.encoding == Encoding::sinteger && inputSignature.encoding == Encoding::sinteger) {
      CopyValueData<SInteger, SInteger>(inData.get(), this);
    }
    if (signature.encoding == Encoding::sinteger && inputSignature.encoding == Encoding::uinteger) {
      CopyValueData<SInteger, UInteger>(inData.get(), this);
    }
    if (signature.encoding == Encoding::uinteger && inputSignature.encoding == Encoding::floating) {
      CopyValueData<UInteger, Floating>(inData.get(), this);
    }
    if (signature.encoding == Encoding::uinteger && inputSignature.encoding == Encoding::sinteger) {
      CopyValueData<UInteger, SInteger>(inData.get(), this);
    }
    if (signature.encoding == Encoding::uinteger && inputSignature.encoding == Encoding::uinteger) {
      CopyValueData<UInteger, UInteger>(inData.get(), this);
    }
  }

  return inData;
}

bool CanLink(DataSignature const& output, DataSignature const& input) {

  if ((input.type == Type::value || input.type == Type::image))
    return true;

  if (input.type != output.type)
    return false;

  if (input.type == Type::buffer)
    return input == output;

  return true;
}
