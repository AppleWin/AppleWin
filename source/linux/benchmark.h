#pragma once

#include <functional>

void VideoBenchmark(std::function<void()> redraw,   // regenerate image and repaint
		    std::function<void()> refresh   // just repaint
		    );
