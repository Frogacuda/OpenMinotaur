#pragma once
#include <glad/gl.h>
#include <map>
#include <string>
#include <vector>
namespace gpu_check {
struct Origin {
    std::string owner, phase;
};
inline std::map<std::pair<int, GLuint>, Origin> live;
inline std::vector<std::string> errors;
inline std::string owner = "display", phase = "setup";
struct Scope {
    std::string oldOwner, oldPhase;
    Scope(std::string who, std::string when) : oldOwner(owner), oldPhase(phase) {
        owner = std::move(who);
        phase = std::move(when);
    }
    ~Scope() {
        owner = oldOwner;
        phase = oldPhase;
    }
};
inline void created(int kind, GLsizei n, const GLuint *names) {
    for (int i = 0; i < n; ++i)
        if (names[i]) {
            auto key = std::make_pair(kind, names[i]);
            if (live.count(key))
                errors.push_back("duplicate live name");
            live[key] = {owner, phase};
        }
}
inline void removed(int kind, GLsizei n, const GLuint *names) {
    for (int i = 0; i < n; ++i)
        if (names[i]) {
            auto key = std::make_pair(kind, names[i]);
            if (!live.erase(key))
                errors.push_back("untracked deletion kind=" + std::to_string(kind) +
                                 " name=" + std::to_string(names[i]) + " owner=" + owner);
        }
}
inline PFNGLGENTEXTURESPROC genTextures;
inline PFNGLDELETETEXTURESPROC deleteTextures;
inline PFNGLGENBUFFERSPROC genBuffers;
inline PFNGLDELETEBUFFERSPROC deleteBuffers;
inline PFNGLGENFRAMEBUFFERSPROC genFramebuffers;
inline PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers;
inline PFNGLGENVERTEXARRAYSPROC genVertexArrays;
inline PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays;
inline PFNGLCREATESHADERPROC createShader;
inline PFNGLDELETESHADERPROC deleteShader;
inline PFNGLCREATEPROGRAMPROC createProgram;
inline PFNGLDELETEPROGRAMPROC deleteProgram;
inline void GLAD_API_PTR gt(GLsizei n, GLuint *p) {
    genTextures(n, p);
    created(1, n, p);
}
inline void GLAD_API_PTR dt(GLsizei n, const GLuint *p) {
    deleteTextures(n, p);
    removed(1, n, p);
}
inline void GLAD_API_PTR gb(GLsizei n, GLuint *p) {
    genBuffers(n, p);
    created(2, n, p);
}
inline void GLAD_API_PTR db(GLsizei n, const GLuint *p) {
    deleteBuffers(n, p);
    removed(2, n, p);
}
inline void GLAD_API_PTR gf(GLsizei n, GLuint *p) {
    genFramebuffers(n, p);
    created(3, n, p);
}
inline void GLAD_API_PTR df(GLsizei n, const GLuint *p) {
    deleteFramebuffers(n, p);
    removed(3, n, p);
}
inline void GLAD_API_PTR gv(GLsizei n, GLuint *p) {
    genVertexArrays(n, p);
    created(4, n, p);
}
inline void GLAD_API_PTR dv(GLsizei n, const GLuint *p) {
    deleteVertexArrays(n, p);
    removed(4, n, p);
}
inline GLuint GLAD_API_PTR cs(GLenum type) {
    auto id = createShader(type);
    created(5, 1, &id);
    return id;
}
inline void GLAD_API_PTR ds(GLuint id) {
    deleteShader(id);
    removed(5, 1, &id);
}
inline GLuint GLAD_API_PTR cp() {
    auto id = createProgram();
    created(6, 1, &id);
    return id;
}
inline void GLAD_API_PTR dp(GLuint id) {
    deleteProgram(id);
    removed(6, 1, &id);
}
inline void install() {
    genTextures = glad_glGenTextures;
    deleteTextures = glad_glDeleteTextures;
    glad_glGenTextures = gt;
    glad_glDeleteTextures = dt;
    genBuffers = glad_glGenBuffers;
    deleteBuffers = glad_glDeleteBuffers;
    glad_glGenBuffers = gb;
    glad_glDeleteBuffers = db;
    genFramebuffers = glad_glGenFramebuffers;
    deleteFramebuffers = glad_glDeleteFramebuffers;
    glad_glGenFramebuffers = gf;
    glad_glDeleteFramebuffers = df;
    genVertexArrays = glad_glGenVertexArrays;
    deleteVertexArrays = glad_glDeleteVertexArrays;
    glad_glGenVertexArrays = gv;
    glad_glDeleteVertexArrays = dv;
    createShader = glad_glCreateShader;
    deleteShader = glad_glDeleteShader;
    glad_glCreateShader = cs;
    glad_glDeleteShader = ds;
    createProgram = glad_glCreateProgram;
    deleteProgram = glad_glDeleteProgram;
    glad_glCreateProgram = cp;
    glad_glDeleteProgram = dp;
}
inline size_t owned(const std::string &who) {
    size_t result = 0;
    for (const auto &pair : live)
        result += pair.second.owner == who;
    return result;
}
} // namespace gpu_check
