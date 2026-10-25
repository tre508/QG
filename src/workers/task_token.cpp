#include "task_token.hpp"

namespace qg {
namespace {
std::atomic<TaskToken::Generation> g_nextGeneration{1};
} // namespace

TaskToken::TaskToken()
    : m_generation(nextGeneration())
{
}

TaskToken::TaskToken(Generation generation)
    : m_generation(generation)
{
}

TaskToken::Generation TaskToken::nextGeneration() noexcept
{
    return g_nextGeneration.fetch_add(1, std::memory_order_relaxed);
}

} // namespace qg