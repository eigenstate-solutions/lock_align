# resource_info

```cpp
struct resource_info;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

One side of a [`violation`](violation.md): the resource a thread held, or
the one it was about to block on.

## Data members

| Member | Type | Description |
|---|---|---|
| `name` | `std::string` | The resource's name |
| `level` | [`level_t`](../level/README.md#level_t) | Its level |
| `kind` | [`resource_kind`](README.md#resource_kind) | Whether it's a mutex or an obligation |

## Member functions

| Function | Description |
|---|---|
| [`describe`](#describe) | Describes the resource in a few words |

---

### describe

```cpp
std::string describe() const;
```

Returns the kind, name and level, for example `mutex 'accounts' (level 1)`
or `obligation 'config' (level 1)`.
