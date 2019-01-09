#pragma once

#include "Marker.h"

namespace clang {
namespace ece459 {

class A1parallel : public Marker {
protected:
    virtual void markAssignment() override;

public:
    explicit A1parallel();
    virtual ~A1parallel();
};

} // namespace ece459
} // namespace clang
