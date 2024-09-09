#include "mlx.h"
#include "spdlog/spdlog.h"
#include "wasinnenv.h"

#ifdef WASMEDGE_PLUGIN_WASI_NN_BACKEND_MLX
#include "converter.h"
#include "prompt.h"
#include "registry.h"
#include "utils.h"
#include <simdjson.h>
#endif
namespace WasmEdge::Host::WASINN::MLX {
#ifdef WASMEDGE_PLUGIN_WASI_NN_BACKEND_MLX
std::string loadBytesFromFile(const std::string &Path) {
  std::ifstream Fs(Path, std::ios::in | std::ios::binary);
  if (Fs.fail()) {
    std::cerr << "Cannot open " << Path << std::endl;
    exit(1);
  }
  std::string Data;
  Fs.seekg(0, std::ios::end);
  const size_t Size = static_cast<size_t>(Fs.tellg());
  Fs.seekg(0, std::ios::beg);
  Data.resize(Size);
  Fs.read(Data.data(), Size);
  return Data;
}
enum AnserSataus {
  STOP,
  WAIT,
  GO,
};
AnserSataus answerSataus(std::string Text, std::string End) {
  if (endsWith(Text, End)) {
    return STOP;
  }
  for (int Idx = 1; Idx < static_cast<int>(End.size()); Idx++) {
    if (endsWith(Text, End.substr(0, Idx))) {
      return WAIT;
    }
  }
  return GO;
}
Expect<WASINN::ErrNo> load(WASINN::WasiNNEnvironment &Env,
                           Span<const Span<uint8_t>> Builders, WASINN::Device,
                           uint32_t &GraphId) noexcept {
  // Add a new graph.
  Env.NNGraph.emplace_back(Backend::MLX);
  auto &GraphRef = Env.NNGraph.back().get<Graph>();
  if (GraphRef.EnableDebugLog) {
    spdlog::info("[WASI-NN] MLX backend: Load."sv);
  }
  std::string TokenizerPath;
  // Parse metadata.
  if (Builders.size() <= 1) {
    spdlog::error(
        "[WASI-NN] MLX backend: Lack model weight or required metadata (tokenizer, model_type)."sv);
    Env.NNGraph.pop_back();
    return ErrNo::InvalidArgument;
  }
  const std::string Metadata = std::string(
      reinterpret_cast<char *>(Builders.back().data()), Builders.back().size());
  simdjson::dom::parser Parser;
  simdjson::dom::element Doc;
  auto ParseError = Parser.parse(Metadata).get(Doc);
  if (ParseError) {
    spdlog::error("[WASI-NN] MLX backend: Parse metadata error"sv);
    Env.NNGraph.pop_back();
    return ErrNo::InvalidEncoding;
  }
  if (Doc.at_key("model_type").error() == simdjson::SUCCESS) {
    std::string_view ModelType;
    auto Err = Doc["model_type"].get<std::string_view>().get(ModelType);
    if (Err) {
      spdlog::error(
          "[WASI-NN] MLX backend: Unable to retrieve the model_type option."sv);
      Env.NNGraph.pop_back();
      return ErrNo::InvalidArgument;
    }
    GraphRef.ModelType = ModelType;
  } else {
    spdlog::error(
        "[WASI-NN] MLX backend: Unable to retrieve the model_type option."sv);
    Env.NNGraph.pop_back();
    return ErrNo::InvalidArgument;
  }
  if (Doc.at_key("enable_debug_log").error() == simdjson::SUCCESS) {
    bool EnableDebugLog;
    auto Err = Doc["enable_debug_log"].get<bool>().get(EnableDebugLog);
    if (Err) {
      spdlog::error(
          "[WASI-NN] MLX backend: Unable to retrieve the enable_debug_log option."sv);
      Env.NNGraph.pop_back();
      return ErrNo::InvalidArgument;
    }
    GraphRef.EnableDebugLog = EnableDebugLog;
  }
  if (Doc.at_key("tokenizer").error() == simdjson::SUCCESS) {
    std::string_view TokenizerPathView;
    auto Err = Doc["tokenizer"].get<std::string_view>().get(TokenizerPathView);
    if (Err) {
      spdlog::error(
          "[WASI-NN] MLX backend: Unable to retrieve the tokenizer option."sv);
      Env.NNGraph.pop_back();
      return ErrNo::InvalidArgument;
    }
    TokenizerPath = TokenizerPathView;
  } else {
    spdlog::error(
        "[WASI-NN] MLX backend: Unable to retrieve the tokenizer option."sv);
    Env.NNGraph.pop_back();
    return ErrNo::InvalidArgument;
  }
  if (Doc.at_key("max_token").error() == simdjson::SUCCESS) {
    uint64_t MaxToken;
    auto Err = Doc["max_token"].get<uint64_t>().get(MaxToken);
    if (Err) {
      spdlog::error(
          "[WASI-NN] MLX backend: Unable to retrieve the max_token option."sv);
      Env.NNGraph.pop_back();
      return ErrNo::InvalidArgument;
    }
    GraphRef.MaxToken = MaxToken;
  }

  // Load tokenizer.
  if (!TokenizerPath.empty()) {
    GraphRef.Tok =
        tokenizers::Tokenizer::FromBlobJSON(loadBytesFromFile(TokenizerPath))
            .release();
  } else {
    spdlog::error("[WASI-NN] MLX backend: Tokenizer path not found."sv);
    Env.NNGraph.pop_back();
    return ErrNo::InvalidArgument;
  }

  // Create Model.
  if (GraphRef.ModelType == "tiny_llama_1.1B_chat_v1.0") {
    GraphRef.Model = tinyLlama11BChatV10();
    GraphRef.Prmopt = TinyLLaMAPrompt();
  } else if (GraphRef.ModelType == "llama_3_8b") {
    GraphRef.Model = llama38b();
    GraphRef.Prmopt = LLaMA3Prompt();
  } else if (GraphRef.ModelType == "llama_2_7b_chat_hf") {
    GraphRef.Model = llama27bChat();
    GraphRef.Prmopt = LLaMA2Prompt();
  } else {
    spdlog::error("[WASI-NN] MLX backend: Model type not supported."sv);
    Env.NNGraph.pop_back();
    return ErrNo::InvalidArgument;
  }

  // Handle the model path.
  for (size_t Idx = 0; Idx < Builders.size() - 1; Idx++) {
    auto Weight = Builders[Idx];
    const std::string BinModel(reinterpret_cast<char *>(Weight.data()),
                               Weight.size());
    spdlog::info("[WASI-NN] MLX BinModel: {}"sv, BinModel.size());
    if (BinModel.size() == 0) {
      Env.NNGraph.pop_back();
      return ErrNo::InvalidArgument;
    }
    std::string ModelFilePath;
    if (BinModel.substr(0, 8) == "preload:"sv) {
      ModelFilePath = BinModel.substr(8);
    } else {
      if (GraphRef.EnableDebugLog) {
        spdlog::info(
            "[WASI-NN][Debug] MLX backend: Model path not found in nn-preload, "
            "write model into a tmpfile."sv);
      }
      // Write model to file.
      // TODO: handle different model format.
      ModelFilePath = "MLX" + std::to_string(Idx) + ".safetensors";
      std::ofstream TempFile(ModelFilePath, std::ios::out | std::ios::binary);
      if (!TempFile) {
        spdlog::error(
            "[WASI-NN] MLX backend: Failed to create the temporary file. "sv);
        Env.NNGraph.pop_back();
        return ErrNo::InvalidArgument;
      }
      TempFile.write(BinModel.data(), BinModel.size());
      TempFile.close();
      if (GraphRef.EnableDebugLog) {
        spdlog::info(
            "[WASI-NN][Debug] MLX backend: Write model into a tmpfile...Done"sv);
      }
    }
    // Load weight.
    if (GraphRef.ModelType == "tiny_llama_1.1B_chat_v1.0") {
      GraphRef.Model->update(llamaToMlxllm(ModelFilePath));
    } else if (GraphRef.ModelType == "llama_3_8b") {
      GraphRef.Model->update(llamaToMlxllm(ModelFilePath));
    } else if (GraphRef.ModelType == "llama_2_7b_chat_hf") {
      GraphRef.Model->update(llamaToMlxllm(ModelFilePath));
    } else {
      spdlog::error("[WASI-NN] MLX backend: Model type not supported."sv);
      Env.NNGraph.pop_back();
      return ErrNo::InvalidArgument;
    }
  }

  return WASINN::ErrNo::Success;
}

Expect<WASINN::ErrNo> initExecCtx(WasiNNEnvironment &Env, uint32_t GraphId,
                                  uint32_t &ContextId) noexcept {
  Env.NNContext.emplace_back(GraphId, Env.NNGraph[GraphId]);
  ContextId = Env.NNContext.size() - 1;
  return ErrNo::Success;
}

Expect<WASINN::ErrNo> setInput(WasiNNEnvironment &Env, uint32_t ContextId,
                               uint32_t Index,
                               const TensorData &Tensor) noexcept {
  auto &CxtRef = Env.NNContext[ContextId].get<Context>();
  auto &GraphRef = Env.NNGraph[CxtRef.GraphId].get<Graph>();
  if (GraphRef.EnableDebugLog) {
    spdlog::info("[WASI-NN] MLX backend: setInput"sv);
  }
  CxtRef.Inputs =
      std::string(reinterpret_cast<const char *>(Tensor.Tensor.data()),
                  Tensor.Tensor.size());
  return WASINN::ErrNo::Success;
}

Expect<WASINN::ErrNo> getOutput(WasiNNEnvironment &Env, uint32_t ContextId,
                                uint32_t Index, Span<uint8_t> OutBuffer,
                                uint32_t &BytesWritten) noexcept {
  auto &CxtRef = Env.NNContext[ContextId].get<Context>();
  auto &GraphRef = Env.NNGraph[CxtRef.GraphId].get<Graph>();
  if (GraphRef.EnableDebugLog) {
    spdlog::info("[WASI-NN] MLX backend: getOutput"sv);
  }
  std::string StringTmp(reinterpret_cast<const char *>(CxtRef.Outputs.data()),
                        CxtRef.Outputs.size());
  std::copy_n(StringTmp.data(), StringTmp.length(), OutBuffer.data());
  BytesWritten = StringTmp.length();
  return WASINN::ErrNo::Success;
}
Expect<WASINN::ErrNo> compute(WasiNNEnvironment &Env,
                              uint32_t ContextId) noexcept {
  auto &CxtRef = Env.NNContext[ContextId].get<Context>();
  auto &GraphRef = Env.NNGraph[CxtRef.GraphId].get<Graph>();
  if (GraphRef.Tok == nullptr) {
    spdlog::error("[WASI-NN] MLX backend: Tokenizer not loaded."sv);
    return ErrNo::InvalidArgument;
  }
  if (GraphRef.EnableDebugLog) {
    spdlog::info("[WASI-NN] MLX backend: compute"sv);
  }
  const std::vector<int32_t> Ids = GraphRef.Tok->Encode(CxtRef.Inputs);
  auto Token =
      mx::array(Ids.data(), {static_cast<int32_t>(Ids.size())}, mx::int32);
  std::vector<int32_t> TokenList;
  std::string Answer;
  int32_t Skip = 0;
  uint64_t TokenCount = 0;
  auto [Y, KVCache] = GraphRef.Model->generate(Token, 0.1);
  while (true) {
    TokenCount++;
    if (TokenCount > GraphRef.MaxToken) {
      break;
    }
    eval(Y);
    std::vector<int32_t> Tokens;
    auto *Data = Y.data<int32_t>();
    for (int Idx = 0; Idx < static_cast<int>(Y.size()); Idx++) {
      Tokens.emplace_back(Data[Idx]);
    }
    // TODO: break when the token is the eos_token_id
    TokenList.insert(TokenList.end(), Tokens.begin(), Tokens.end());
    Answer = GraphRef.Tok->Decode(TokenList);
    const AnserSataus Status = answerSataus(Answer, GraphRef.Prmopt.TextEnd);
    if (Status == STOP) {
      break;
    }
    if (Status == GO) {
      CxtRef.Outputs += Answer.substr(Skip);
      Skip = Answer.size();
    }
    auto [NY, NKVCache] =
        GraphRef.Model->nextGenerate(Y, GraphRef.Temp, KVCache);
    Y = NY, KVCache = NKVCache;
  }
  return WASINN::ErrNo::Success;
}
#else
namespace {
Expect<WASINN::ErrNo> reportBackendNotSupported() noexcept {
  spdlog::error("[WASI-NN] MLX backend is not built. use "
                "-WASMEDGE_PLUGIN_WASI_NN_BACKEND=\"MLX\" to build it."sv);
  return WASINN::ErrNo::InvalidArgument;
}
} // namespace

Expect<WASINN::ErrNo> load(WASINN::WasiNNEnvironment &,
                           Span<const Span<uint8_t>>, WASINN::Device,
                           uint32_t &) noexcept {
  return reportBackendNotSupported();
}
Expect<WASINN::ErrNo> initExecCtx(WASINN::WasiNNEnvironment &, uint32_t,
                                  uint32_t &) noexcept {
  return reportBackendNotSupported();
}
Expect<WASINN::ErrNo> setInput(WASINN::WasiNNEnvironment &, uint32_t, uint32_t,
                               const TensorData &) noexcept {
  return reportBackendNotSupported();
}
Expect<WASINN::ErrNo> getOutput(WASINN::WasiNNEnvironment &, uint32_t, uint32_t,
                                Span<uint8_t>, uint32_t &) noexcept {
  return reportBackendNotSupported();
}
Expect<WASINN::ErrNo> compute(WASINN::WasiNNEnvironment &, uint32_t) noexcept {
  return reportBackendNotSupported();
}
#endif

} // namespace WasmEdge::Host::WASINN::MLX