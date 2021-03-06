// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <string>

#include "atom/common/native_mate_converters/callback.h"
#include "atom/common/native_mate_converters/file_path_converter.h"
#include "atom/common/native_mate_converters/gurl_converter.h"
#include "atom/common/native_mate_converters/string16_converter.h"
#include "atom/common/node_includes.h"
#include "atom/common/platform_util.h"
#include "atom/common/promise_util.h"
#include "native_mate/dictionary.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/shortcut.h"

namespace mate {

template <>
struct Converter<base::win::ShortcutOperation> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     base::win::ShortcutOperation* out) {
    std::string operation;
    if (!ConvertFromV8(isolate, val, &operation))
      return false;
    if (operation.empty() || operation == "create")
      *out = base::win::SHORTCUT_CREATE_ALWAYS;
    else if (operation == "update")
      *out = base::win::SHORTCUT_UPDATE_EXISTING;
    else if (operation == "replace")
      *out = base::win::SHORTCUT_REPLACE_EXISTING;
    else
      return false;
    return true;
  }
};

}  // namespace mate
#endif

namespace {

void OnOpenExternalFinished(const v8::Global<v8::Context>& context,
                            scoped_refptr<atom::util::Promise> promise,
                            const std::string& error) {
  v8::Isolate* isolate = promise->isolate();
  mate::Locker locker(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::MicrotasksScope script_scope(isolate,
                                   v8::MicrotasksScope::kRunMicrotasks);
  v8::Context::Scope context_scope(
      v8::Local<v8::Context>::New(isolate, context));

  if (error.empty())
    promise->Resolve();
  else
    promise->RejectWithErrorMessage(error.c_str());
}

bool OpenExternalSync(
#if defined(OS_WIN)
    const base::string16& url,
#else
    const GURL& url,
#endif
    mate::Arguments* args) {
  platform_util::OpenExternalOptions options;
  if (args->Length() >= 2) {
    mate::Dictionary obj;
    if (args->GetNext(&obj)) {
      obj.Get("activate", &options.activate);
      obj.Get("workingDirectory", &options.working_dir);
    }
  }

  return platform_util::OpenExternal(url, options);
}

v8::Local<v8::Promise> OpenExternal(
#if defined(OS_WIN)
    const base::string16& url,
#else
    const GURL& url,
#endif
    mate::Arguments* args) {
  scoped_refptr<atom::util::Promise> promise =
      new atom::util::Promise(args->isolate());

  platform_util::OpenExternalOptions options;
  if (args->Length() >= 2) {
    mate::Dictionary obj;
    if (args->GetNext(&obj)) {
      obj.Get("activate", &options.activate);
      obj.Get("workingDirectory", &options.working_dir);
    }
  }

  v8::Global<v8::Context> context(args->isolate(),
                                  args->isolate()->GetCurrentContext());
  platform_util::OpenExternal(
      url, options,
      base::Bind(&OnOpenExternalFinished, std::move(context), promise));

  return promise->GetHandle();
}

#if defined(OS_WIN)
bool WriteShortcutLink(const base::FilePath& shortcut_path,
                       mate::Arguments* args) {
  base::win::ShortcutOperation operation = base::win::SHORTCUT_CREATE_ALWAYS;
  args->GetNext(&operation);
  mate::Dictionary options = mate::Dictionary::CreateEmpty(args->isolate());
  if (!args->GetNext(&options)) {
    args->ThrowError();
    return false;
  }

  base::win::ShortcutProperties properties;
  base::FilePath path;
  base::string16 str;
  int index;
  if (options.Get("target", &path))
    properties.set_target(path);
  if (options.Get("cwd", &path))
    properties.set_working_dir(path);
  if (options.Get("args", &str))
    properties.set_arguments(str);
  if (options.Get("description", &str))
    properties.set_description(str);
  if (options.Get("icon", &path) && options.Get("iconIndex", &index))
    properties.set_icon(path, index);
  if (options.Get("appUserModelId", &str))
    properties.set_app_id(str);

  base::win::ScopedCOMInitializer com_initializer;
  return base::win::CreateOrUpdateShortcutLink(shortcut_path, properties,
                                               operation);
}

v8::Local<v8::Value> ReadShortcutLink(mate::Arguments* args,
                                      const base::FilePath& path) {
  using base::win::ShortcutProperties;
  mate::Dictionary options = mate::Dictionary::CreateEmpty(args->isolate());
  base::win::ScopedCOMInitializer com_initializer;
  base::win::ShortcutProperties properties;
  if (!base::win::ResolveShortcutProperties(
          path, ShortcutProperties::PROPERTIES_ALL, &properties)) {
    args->ThrowError("Failed to read shortcut link");
    return v8::Null(args->isolate());
  }
  options.Set("target", properties.target);
  options.Set("cwd", properties.working_dir);
  options.Set("args", properties.arguments);
  options.Set("description", properties.description);
  options.Set("icon", properties.icon);
  options.Set("iconIndex", properties.icon_index);
  options.Set("appUserModelId", properties.app_id);
  return options.GetHandle();
}
#endif

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  mate::Dictionary dict(context->GetIsolate(), exports);
  dict.SetMethod("showItemInFolder", &platform_util::ShowItemInFolder);
  dict.SetMethod("openItem", &platform_util::OpenItem);
  dict.SetMethod("openExternalSync", &OpenExternalSync);
  dict.SetMethod("openExternal", &OpenExternal);
  dict.SetMethod("moveItemToTrash", &platform_util::MoveItemToTrash);
  dict.SetMethod("beep", &platform_util::Beep);
#if defined(OS_WIN)
  dict.SetMethod("writeShortcutLink", &WriteShortcutLink);
  dict.SetMethod("readShortcutLink", &ReadShortcutLink);
#endif
}

}  // namespace

NODE_BUILTIN_MODULE_CONTEXT_AWARE(atom_common_shell, Initialize)
