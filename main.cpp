#include "include/app.h"

#define TEST_CHUNKING

#ifdef TEST_CHUNKING
#include <iostream>
#include "chunker.h"
#include "sourceproc.h"
#endif


int main(int argc, char *argv[]) {

#ifdef TEST_CHUNKING

  if (argc > 1 && std::string(argv[1]) == "test_chunking") {
    App app("settings.json");

    const auto &chunker = app.chunker();
    std::string testCode = R"(#include <iostream>
    class MyClass {
    public:
        MyClass() {}
        void myFunction(int x) {
            if (x > 0) {
                std::cout << "Positive" << std::endl;
            } else {
                std::cout << "Non-positive" << std::endl;
            }
        }
    };
)";
  
    std::string testText = R"(# Project Title
)";

    const auto &srcProc = app.sourceProcessor();
    auto src = const_cast<SourceProcessor &>(srcProc).collectSources();
    if (4 < src.size()) {
      testCode = src[2].content;
    }

    auto codeChunks = chunker.chunkText(testCode);
    auto textChunks = chunker.chunkText(testText);
  
    std::cout << "Code Chunks:";
    for (const auto &chunk : codeChunks) {
      std::cout << "\n----- size="<< chunk.metadata.tokenCount << "\n" << chunk.text << "\n";
    }
  
    std::cout << "\n\nText Chunks:";
    for (const auto &chunk : textChunks) {
      std::cout << "\n----- size="<< chunk.metadata.tokenCount << "\n" << chunk.text << "\n";
    }
  
    return 0;
  }

#endif

  return App::run(argc, argv);
}