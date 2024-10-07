#pragma once
#include <vector>
#include <iostream>
#include <set>
#include <unordered_map>
#include <tuple>
#include <concepts>
#include <cassert>
#include "Profiler/Profiler.h"

// Definitions
namespace ECS
{
#define ENTITY(...) struct ECS::Entity : ECS::EntityTemplate<__VA_ARGS__> {Entity(uint32_t index = -1) : ECS::EntityTemplate<__VA_ARGS__>(index){}}

    template <typename TComponent>
    class Component;

    template <typename TComponent>
    concept ComponentDerived = std::is_base_of_v<Component<TComponent>, TComponent>;

    template<ComponentDerived... TComponents>
    struct ComponentTable
    {
        ComponentTable();
    private:

        template <ComponentDerived TComponent>
        uint64_t GetIndex() const;

        template <ComponentDerived TComponent>
        void SetIndex(uint64_t index);

        template <ComponentDerived TComponent>
        void SetEmpty();

        template <ComponentDerived TComponent>
        bool IsEmpty();

        template <ComponentDerived TComponent>
        struct ComponentIndex { TComponent::TIndex index; };

        std::tuple<ComponentIndex<TComponents>...> table;

        template<ComponentDerived... UComponents>
        friend class EntityTemplate;

        template <ComponentDerived TComponent>
        friend class System;
    };

#define REGISTER_SYSTEMS(componentType, ...)template <> struct ECS::SystemsCollection<componentType>{using Systems = std::tuple<__VA_ARGS__>;}

    template<ComponentDerived TComponent>
    struct SystemsCollection
    {
        using Systems = std::tuple<>;
    };

    class Entity;
    template<ComponentDerived... TComponents>
    struct EntityTemplate
    {
        EntityTemplate(uint32_t index = -1);
        EntityTemplate(EntityTemplate&& other) noexcept;
        EntityTemplate(const EntityTemplate& other);

        EntityTemplate& operator=(EntityTemplate&& other) noexcept;
        EntityTemplate& operator=(const EntityTemplate& other);

        template <typename TEntity = Entity, ComponentDerived... UComponents>
        static TEntity AddEntity(UComponents&&... components);

        template <ComponentDerived TComponent>
        TComponent& AddComponent(TComponent&& component);

        template <ComponentDerived TComponent>
        bool HasComponent() const;

        template <ComponentDerived TComponent>
        TComponent& GetComponent()const;

        template <ComponentDerived TComponent>
        EntityTemplate& DestroyComponent();

        void Destroy();

    protected:
        ~EntityTemplate();

    private:
        uint32_t index;

        static std::unordered_map<uint32_t, std::set<EntityTemplate*>> entityReferances;
        static std::vector<ComponentTable<TComponents...>> entities;

        template <typename TComponent>
        friend class Component;
        template <ComponentDerived TComponent>
        friend class System;
    };

    template <typename TComponent>
    class Component
    {
    public:
        using TIndex = uint32_t;
        using Parent = Component<TComponent>;
        Component();

        template<typename TEntity = Entity>
        TEntity GetEntity() const;

        template <ComponentDerived UComponent, typename TEntity = Entity>
        bool HasComponent() const;

        template<ComponentDerived UComponent, typename TEntity = Entity>
        UComponent& GetComponent() const;

        template<ComponentDerived UComponent, typename TEntity = Entity>
        UComponent& AddComponent(UComponent&& component);

        template <ComponentDerived UComponent, typename TEntity = Entity>
        void DestroyComponent();

        template<typename TEntity = Entity>
        void Destroy();

    private:
        uint32_t entityIndex;

        template<ComponentDerived... TComponents>
        friend class EntityTemplate;
        template<ComponentDerived UComponent>
        friend class System;
    };

    template <ComponentDerived TComponent>
    class System
    {
    public:
        static uint64_t AddComponent(TComponent&& component);
        static bool IsValid(uint64_t index);
        static TComponent& GetComponent(uint64_t index);
        template <typename TEntity>
        static void DestroyComponent(uint64_t index);

    protected:
        static std::vector<TComponent> components;
        template<ComponentDerived... TComponents>
        friend class EntityTemplate;
    };
}

// Implementations
namespace ECS
{
    template <typename... TSystems, ComponentDerived TComponent>
    TComponent& InvokeAddComponent(std::tuple<TSystems...> tuple, TComponent& component)
    {
        (TSystems::Add(component), ...);

        return component;
    }

    template <typename... TSystems, ComponentDerived TComponent>
    TComponent& InvokeDestroyComponent(std::tuple<TSystems...> tuple, TComponent& component)
    {
        (TSystems::Destroy(component), ...);

        return component;
    }

#pragma region ComponentTable
    template<ComponentDerived... TComponents>
    ComponentTable<TComponents...>::ComponentTable()
    {
        (SetEmpty<TComponents>(), ...);
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    uint64_t ComponentTable<TComponents...>::GetIndex() const
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        return std::get<ComponentIndex<TComponent>>(table).index;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    void ComponentTable<TComponents...>::SetIndex(uint64_t index)
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        assert(("Component table index overflow", index < (typename TComponent::TIndex) - 1));
        std::get<ComponentIndex<TComponent>>(table).index = index;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    void ComponentTable<TComponents...>::SetEmpty()
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        std::get<ComponentIndex<TComponent>>(table).index = -1;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    bool ComponentTable<TComponents...>::IsEmpty()
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        return std::get<ComponentIndex<TComponent>>(table).index == (typename TComponent::TIndex) - 1;
    }
#pragma endregion

#pragma region Entity
    template<ComponentDerived... TComponents>
    std::unordered_map<uint32_t, std::set<EntityTemplate<TComponents...>*>> EntityTemplate<TComponents...>::entityReferances;

    template<ComponentDerived... TComponents>
    std::vector<ComponentTable<TComponents...>> EntityTemplate<TComponents...>::entities;

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>::EntityTemplate(uint32_t index) :index(index)
    {
        assert(("Invalid Index", index == -1 || index < entities.size()));
        if (index != -1)
            entityReferances[index].insert(this);
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>::EntityTemplate(EntityTemplate<TComponents...>&& other) noexcept
    {
        if (index != -1)
            entityReferances.at(index).erase(this);

        index = other.index;

        entityReferances.at(index).erase(&other);
        entityReferances[index].insert(this);
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>::EntityTemplate(const EntityTemplate<TComponents...>& other)
    {
        if (index != -1)
            entityReferances.at(index).erase(this);

        index = other.index;

        entityReferances[index].insert(this);
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>& EntityTemplate<TComponents...>::operator=(EntityTemplate<TComponents...>&& other) noexcept
    {
        if (&other == this) return *this;

        if (index != -1)
            entityReferances.at(index).erase(this);

        index = other.index;

        entityReferances.at(index).erase(&other);
        entityReferances[index].insert(this);

        return *this;
    }
    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>& EntityTemplate<TComponents...>::operator=(const EntityTemplate<TComponents...>& other)
    {
        if (&other == this) return *this;

        if (index != -1)
            entityReferances.at(index).erase(this);

        index = other.index;

        entityReferances.insert(this);

        return *this;
    }

    template<ComponentDerived... TComponents>
    template <typename TEntity, ComponentDerived... UComponents>
    TEntity EntityTemplate<TComponents...>::AddEntity(UComponents&&... components)
    {
        TEntity entity;
        entity.index = entities.size();
        if (entities.size() >= entities.capacity())
            entities.reserve(entities.size() * 1.71f);

        entities.resize(entities.size() + 1);
        entityReferances[entity.index].insert(&entity);

        (entity.template AddComponent<UComponents>(std::move(components)), ...);

        return entity;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    TComponent& EntityTemplate<TComponents...>::AddComponent(TComponent&& component)
    {
        assert(("Invalid Entity", this->index != -1));
        assert(("Cannot add multiple components of same type", entities[this->index].template IsEmpty<TComponent>()));
        component.entityIndex = this->index;
        uint64_t index = System<TComponent>::AddComponent(std::move(component));
        entities[this->index].template SetIndex<TComponent>(index);

        return InvokeAddComponent(typename SystemsCollection<TComponent>::Systems(), GetComponent<TComponent>());
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    bool EntityTemplate<TComponents...>::HasComponent() const
    {
        assert(("Invalid Entity", this->index != -1));

        uint64_t index = entities[this->index].template GetIndex<TComponent>();
        return System<TComponent>::IsValid(index);
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    TComponent& EntityTemplate<TComponents...>::GetComponent() const
    {
        assert(("Invalid Entity", this->index != -1));
        assert(("Cannot get nonexistant component", HasComponent<TComponent>()));

        return System<TComponent>::GetComponent(entities[this->index].template GetIndex<TComponent>());
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    EntityTemplate<TComponents...>& EntityTemplate<TComponents...>::DestroyComponent()
    {
        assert(("Invalid Entity", this->index != -1));
        assert(("Cannot destroy nonexistant component", HasComponent<TComponent>()));

        auto componentIndex = entities[this->index].template GetIndex<TComponent>();
        System<TComponent>::template DestroyComponent<EntityTemplate<TComponents...>>(componentIndex);
        return *this;
    }

    template<ComponentDerived... TComponents>
    void EntityTemplate<TComponents...>::Destroy()
    {
        assert(("Invalid Entity", this->index != -1));

        uint64_t other = entities.size() - 1;
        uint64_t index = this->index;
        if (other != index)
        {
            EntityTemplate o(other);

            ((o.HasComponent<TComponents>() ? o.GetComponent<TComponents>().entityIndex = index : uint32_t()), ...);

            std::swap(entities[index], entities[other]);
        }
        ((HasComponent<TComponents>() ? DestroyComponent<TComponents>() : *this), ...);
        entities.pop_back();

        for (auto&& i : entityReferances[index])
            i->index = -1;

        for (auto&& i : entityReferances[other])
            i->index = index;
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...> ::~EntityTemplate()
    {
        if (index == -1)
            return;

        if (entityReferances.at(index).size() > 1)
            return;

        if (!entityReferances.at(index).contains(this))
            return;

        Destroy();
        entityReferances.at(index).erase(this);
    }
#pragma endregion

#pragma region Component

    template <typename TComponent>
    Component<TComponent>::Component() :entityIndex(-1) {}

    template <typename TComponent>
    template <typename TEntity>
    TEntity Component<TComponent>::GetEntity() const
    {
        return TEntity(entityIndex);
    }

    template <typename TComponent>
    template <ComponentDerived UComponent, typename TEntity>
    bool Component<TComponent>::HasComponent() const
    {
        TEntity entityReferance(-1);
        entityReferance.index = entityIndex;
        bool r = entityReferance.template HasComponent<UComponent>();
        entityReferance.index = -1;

        return r;
    }

    template <typename TComponent>
    template<ComponentDerived UComponent, typename TEntity>
    UComponent& Component<TComponent>::GetComponent() const
    {
        TEntity entityReferance(-1);
        entityReferance.index = entityIndex;
        UComponent& r = entityReferance.template GetComponent<UComponent>();
        entityReferance.index = -1;

        return r;
    }

    template <typename TComponent>
    template<ComponentDerived UComponent, typename TEntity>
    UComponent& Component<TComponent>::AddComponent(UComponent&& component)
    {
        TEntity entityReferance(-1);
        entityReferance.index = entityIndex;
        UComponent& r = entityReferance.template AddComponent<UComponent>(std::move(component));
        entityReferance.index = -1;

        return r;
    }

    template <typename TComponent>
    template <ComponentDerived UComponent, typename TEntity>
    void Component<TComponent>::DestroyComponent()
    {
        TEntity entityReferance(-1);
        entityReferance.index = entityIndex;
        entityReferance.template DestroyComponent<UComponent>();
        entityReferance.index = -1;
    }

    template <typename TComponent>
    template<typename TEntity>
    void Component<TComponent>::Destroy()
    {
        TEntity entityReferance(-1);
        entityReferance.index = entityIndex;
        entityReferance.template DestroyComponent<TComponent>();
        entityReferance.index = -1;
    }
#pragma endregion

#pragma region System
    template <ComponentDerived TComponent>
    std::vector<TComponent> System<TComponent>::components;

    template <ComponentDerived TComponent>
    uint64_t System<TComponent>::AddComponent(TComponent&& component)
    {
        uint64_t index = components.size();
        components.emplace_back(std::move(component));
        assert(("Make sure to call parent assignement operator in component assignment operator", components[index].entityIndex == component.entityIndex));

        return index;
    }

    template <ComponentDerived TComponent>
    bool System<TComponent>::IsValid(uint64_t index)
    {
        return index < components.size();
    }

    template <ComponentDerived TComponent>
    TComponent& System<TComponent>::GetComponent(uint64_t index)
    {
        assert(("Invalid Component", System<TComponent>::IsValid(index)));
        return components[index];
    }

    template <ComponentDerived TComponent>
    template<typename TEntity>
    void System<TComponent>::DestroyComponent(uint64_t index)
    {
        assert(("Invalid Component", System<TComponent>::IsValid(index)));

        InvokeDestroyComponent(typename SystemsCollection<TComponent>::Systems(), components[index]);

        uint64_t other = components.size() - 1;
        TEntity::entities[components[index].entityIndex].template SetEmpty<TComponent>();
        TEntity::entities[components[other].entityIndex].template SetIndex<TComponent>(index);

        components[index] = std::move(components[other]);
        assert(("Make sure to call parent assignement operator in component assignment operator", components[index].entityIndex == components[other].entityIndex));

        components.pop_back();
    }
#pragma endregion
}