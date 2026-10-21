#include <gtest/gtest.h>

#include "core/System.h"
#include "core/ecs/Registry.h"

#include <memory>
#include <vector>

namespace {

class CountingSystem : public Engine::ECS::ISystem {
public:
    CountingSystem(int32_t priority, std::vector<int32_t>* order)
        : m_order(order) {
        SetPriority(priority);
    }

    void Update(float, Engine::ECS::Registry&, uint64_t) override {
        m_order->push_back(GetPriority());
    }

private:
    std::vector<int32_t>* m_order;
};

} // namespace

TEST(SystemManagerTest, UpdatesEnabledSystemsInPriorityOrder) {
    Engine::ECS::SystemManager systems;
    Engine::ECS::Registry registry;
    std::vector<int32_t> order;

    auto late = std::make_shared<CountingSystem>(10, &order);
    auto early = std::make_shared<CountingSystem>(-5, &order);
    auto skipped = std::make_shared<CountingSystem>(0, &order);
    skipped->SetEnabled(false);

    systems.RegisterSystem(late);
    systems.RegisterSystem(early);
    systems.RegisterSystem(skipped);
    systems.Update(0.016f, registry, 1);

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], -5);
    EXPECT_EQ(order[1], 10);
}
