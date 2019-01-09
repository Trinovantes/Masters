#pragma once

#include "Marker.h"

namespace clang {
namespace ece459 {

class A1nbio : public Marker {
protected:
    virtual void markAssignment() override;

public:
    explicit A1nbio();
    virtual ~A1nbio();
};

} // namespace ece459
} // namespace clang
