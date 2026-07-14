#include "TestComponent.h"
#include "components/scene.h"
#include "components/resolved_mount_service.h"
#include "engine/core/parse.h"
#include "engine/document_runtime.h"
#include "misc/logger.h"
#include "engine/core/ast_node.h"

namespace ldt {

void TestComponent::onAttach(Scene* scene, const std::string& parentId) {
    if (!scene || !scene->getControlManager()) return;

    LDTParser parser;
    AstSnippet snippet;
    try {
        snippet = parser.parse(R"ui(
@style {
    .test-box {
        width: 200px;
        height: 100px;
        background-color: #4a90e2;
        border: 2px solid #ffffff;
        border-radius: 8px;
        padding: 10px;
    }
    .test-text {
        //color: #ffffff;
        font-size: 16px;
    }
}
@layout {
    .test-box {
        display: flex;
        justify-content: center;
        align-items: center;flex-direction: row;
    }
}
button:btnAddComp(){text(value="11")}
panel:testPanel(class="test-box") {
    text:testLabel(class="test-text", value="Add")
    text:testLabel(class="test-text", value="Add")
}
)ui");
    }
    catch (const std::exception& ex) {
        LDT_ERROR("TestComponent parse error: " << ex.what());
        return;
    }

    auto mounted = ResolvedMountService::MountSnippetUnderId(scene->getResolvedTree(), parentId, snippet, scene->getDocumentRuntime());
    if (!mounted) {
        LDT_ERROR("TestComponent mount failed");
        return;
    }
    
    LDT_LOG("TestComponent attached to " << parentId);
}

} // namespace ldt
