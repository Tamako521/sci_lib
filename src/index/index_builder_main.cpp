#include "index/index_builder.hpp"

int main()
{
    indexed::IndexBuilder builder;
    return builder.build() ? 0 : 1;
}
