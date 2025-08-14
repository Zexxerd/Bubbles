#if !defined(KEY_H)
#define KEY_H

#define single_press(key, prev) (key && !prev)
#define single_release(key, prev) (!key && prev)

#endif // KEY_H