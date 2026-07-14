#pragma once
#include "components/ui_component.h"
#include <string>

namespace ldt {

class TestComponent : public UIComponent {
public:
    void onAttach(Scene* scene, const std::string& parentId) override;
};

} // namespace ldt
