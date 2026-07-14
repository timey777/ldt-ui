#include "compositor.h"
#include "render/drawer.h"
#include "components/scene.h"
#include <fstream>
#include <iostream>
#include "misc/logger.h"
#include <stdexcept>
namespace ldt {

	Compositor::Compositor()
		: renderer_(nullptr), isDirty_(true) {
	}

	Compositor::~Compositor() {
	}


	void Compositor::setRenderer(Renderer* renderer) {
		renderer_ = renderer;
	}

	Renderer* Compositor::getRenderer() const {
		return renderer_;
	}

	void Compositor::submitDisplayList(const DisplayList& displayList) {
		displayLists_.push_back(displayList);
		isDirty_ = true;
	}

	void Compositor::submitDisplayList(DisplayList&& displayList) {
		displayList.compact();
		displayLists_.push_back(std::move(displayList));
		isDirty_ = true;
	}

	void Compositor::submitOverlayDisplayList(const DisplayList& overlay) {
		overlayDisplayList_ = overlay;
		isDirty_ = true;
	}

	void Compositor::submitOverlayDisplayList(DisplayList&& overlay) {
		overlay.compact();
		overlayDisplayList_ = std::move(overlay);
		isDirty_ = true;
	}

	void Compositor::paintAll() {
		if (!renderer_) return;

		renderer_->updateImageLoadQueue();

		for (const auto& displayList : displayLists_) {
			renderer_->render(displayList);
		}
		if (overlayDisplayList_.size() > 0) {
			renderer_->render(overlayDisplayList_);
		}

		isDirty_ = false;
	}

	void Compositor::paintAllTo(const ui::Rect& target, bool presentAfter) {
		if (!renderer_) return;

		renderer_->updateImageLoadQueue();

		for (const auto& displayList : displayLists_) {
			renderer_->render(displayList, target);
		}
		if (overlayDisplayList_.size() > 0) {
			renderer_->render(overlayDisplayList_, target);
		}

		isDirty_ = false;
	}

	void Compositor::clear() {
		displayLists_.clear();
		isDirty_ = true;
	}

	bool Compositor::saveDisplayListsToFile(const std::string& path) const {
		try {
			std::ofstream ofs(path, std::ios::out | std::ios::trunc);
			if (!ofs) return false;
			for (const auto& dl : displayLists_) {
				ofs << "LIST_BEGIN\n";
				ofs << dl.serializeToText();
				ofs << "LIST_END\n";
			}
			ofs << "OVERLAY_BEGIN\n";
			ofs << overlayDisplayList_.serializeToText();
			ofs << "OVERLAY_END\n";
			return true;
		}
		catch (...) {
			LDT_ERROR("Compositor::saveDisplayListsToFile failed");
			return false;
		}
	}

	bool Compositor::loadDisplayListsFromFile(const std::string& path) {
		try {
			std::ifstream ifs(path);
			if (!ifs) return false;
			std::string line;
			std::vector<DisplayList> newLists;
			bool inList = false;
			bool inOverlay = false;
			std::ostringstream cur;
			while (std::getline(ifs, line)) {
				if (line == "LIST_BEGIN") { inList = true; cur.str(""); cur.clear(); continue; }
				if (line == "LIST_END") { if (inList) { newLists.push_back(DisplayList::deserializeFromText(cur.str())); } inList = false; cur.str(""); cur.clear(); continue; }
				if (line == "OVERLAY_BEGIN") { inOverlay = true; cur.str(""); cur.clear(); continue; }
				if (line == "OVERLAY_END") { if (inOverlay) { overlayDisplayList_ = DisplayList::deserializeFromText(cur.str()); } inOverlay = false; cur.str(""); cur.clear(); continue; }
				if (inList || inOverlay) {
					cur << line << '\n';
				}
			}
			displayLists_.swap(newLists);
			isDirty_ = true;
			return true;
		}
		catch (...) {
			LDT_ERROR("Compositor::loadDisplayListsFromFile failed");
			return false;
		}
	}

	void Compositor::markDirty(float x, float y, float w, float h) {
		isDirty_ = true;
	}

	void Compositor::renderScene(Scene* scene) {
		if (!scene) return;
		DisplayList displayList;
		scene->render(displayList);
		clear();
		submitDisplayList(std::move(displayList));
	}

} // namespace ldt
