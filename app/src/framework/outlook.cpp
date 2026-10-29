#include "framework/outlook.h"

#include <st/util/strings.h>

#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

#include <cstdio>
#include <cwctype>
#include <initializer_list>
#include <utility>
#include <vector>
#endif

namespace st::app::outlook {

#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)

namespace {

std::wstring widen(const std::string &utf8) { return util::pathFromUtf8(utf8).wstring(); }

/// COM for the calling thread (worker threads have none yet).
class ComScope {
public:
  ComScope() : mResult(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
  ~ComScope() {
    if (SUCCEEDED(mResult))
      CoUninitialize();
  }
  ComScope(const ComScope &) = delete;
  ComScope &operator=(const ComScope &) = delete;

private:
  HRESULT mResult;
};

class Variant {
public:
  Variant() { VariantInit(&mValue); }
  explicit Variant(const std::wstring &text) : Variant() {
    mValue.vt = VT_BSTR;
    mValue.bstrVal = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
  }
  explicit Variant(int number) : Variant() {
    mValue.vt = VT_I4;
    mValue.lVal = number;
  }
  static Variant boolean(bool value) {
    Variant v;
    v.mValue.vt = VT_BOOL;
    v.mValue.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    return v;
  }
  Variant(Variant &&other) noexcept : Variant() { std::swap(mValue, other.mValue); }
  ~Variant() { VariantClear(&mValue); }
  Variant(const Variant &) = delete;
  Variant &operator=(const Variant &) = delete;

  VARIANT *get() { return &mValue; }
  IDispatch *dispatch() const { return mValue.vt == VT_DISPATCH ? mValue.pdispVal : nullptr; }
  std::wstring text() const { return mValue.vt == VT_BSTR && mValue.bstrVal ? std::wstring(mValue.bstrVal, SysStringLen(mValue.bstrVal)) : std::wstring(); }

private:
  VARIANT mValue;
};

/// Calls a method or property of an automation object. Arguments in natural order.
HRESULT invoke(IDispatch *object, WORD flags, const wchar_t *name, Variant *result, std::initializer_list<Variant *> args = {}) {
  if (!object)
    return E_POINTER;
  DISPID id = 0;
  LPOLESTR member = const_cast<LPOLESTR>(name);
  if (const HRESULT hr = object->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_USER_DEFAULT, &id); FAILED(hr))
    return hr;
  // IDispatch expects the arguments in reverse order
  std::vector<VARIANT> argv(args.size());
  std::size_t i = args.size();
  for (Variant *arg : args)
    argv[--i] = *arg->get();
  DISPPARAMS params{argv.data(), nullptr, static_cast<UINT>(argv.size()), 0};
  DISPID put = DISPID_PROPERTYPUT;
  if (flags & DISPATCH_PROPERTYPUT) {
    params.rgdispidNamedArgs = &put;
    params.cNamedArgs = 1;
  }
  return object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, result ? result->get() : nullptr, nullptr, nullptr);
}

HRESULT put(IDispatch *object, const wchar_t *name, const std::string &utf8) {
  Variant value(widen(utf8));
  return invoke(object, DISPATCH_PROPERTYPUT, name, nullptr, {&value});
}

util::Error failure(const std::string &what, HRESULT hr) {
  char code[16];
  std::snprintf(code, sizeof(code), "0x%08lX", static_cast<unsigned long>(hr));
  return util::Error{what + " (" + code + ")"};
}

/// The text as HTML paragraphs (Outlook keeps the signature below it).
std::wstring bodyAsHtml(const std::string &body) {
  std::string html = "<div style=\"font-family:Calibri,Arial,sans-serif;font-size:11pt\">";
  for (const auto &line : util::splitLines(body))
    html += (line.empty() ? std::string("&nbsp;") : util::htmlEscape(line)) + "<br>";
  return widen(html + "</div>");
}

/// Inserts `html` right after the opening <body ...> tag of `document`.
std::wstring insertAtBodyStart(std::wstring document, const std::wstring &html) {
  std::wstring lower = document;
  for (auto &c : lower)
    c = static_cast<wchar_t>(std::towlower(c));
  const auto body = lower.find(L"<body");
  const auto close = body == std::wstring::npos ? std::wstring::npos : lower.find(L'>', body);
  if (close == std::wstring::npos)
    return html + document;
  document.insert(close + 1, html);
  return document;
}

} // namespace

util::Status createDraft(const exporting::MailDraft &draft) {
  ComScope com;
  CLSID clsid;
  if (FAILED(CLSIDFromProgID(L"Outlook.Application", &clsid)))
    return std::unexpected(util::Error{"Outlook (klassisch) ist nicht installiert"});
  IDispatch *application = nullptr;
  if (const HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void **>(&application)); FAILED(hr))
    return std::unexpected(failure("Outlook konnte nicht gestartet werden", hr));

  Variant mailItem;
  Variant olMailItem(0);
  HRESULT hr = invoke(application, DISPATCH_METHOD, L"CreateItem", &mailItem, {&olMailItem});
  application->Release();
  IDispatch *mail = mailItem.dispatch();
  if (FAILED(hr) || !mail)
    return std::unexpected(failure("Outlook: neue E-Mail konnte nicht angelegt werden", hr));

  if (!draft.to.empty() && FAILED(hr = put(mail, L"To", draft.to)))
    return std::unexpected(failure("Outlook: Empfänger ungültig", hr));
  if (!draft.cc.empty() && FAILED(hr = put(mail, L"CC", draft.cc)))
    return std::unexpected(failure("Outlook: CC ungültig", hr));
  if (FAILED(hr = put(mail, L"Subject", draft.subject)))
    return std::unexpected(failure("Outlook: Betreff konnte nicht gesetzt werden", hr));

  Variant attachments;
  if (FAILED(hr = invoke(mail, DISPATCH_PROPERTYGET, L"Attachments", &attachments)) || !attachments.dispatch())
    return std::unexpected(failure("Outlook: Anhänge nicht verfügbar", hr));
  for (const auto &file : draft.attachments) {
    Variant path(file.wstring());
    if (FAILED(hr = invoke(attachments.dispatch(), DISPATCH_METHOD, L"Add", nullptr, {&path})))
      return std::unexpected(failure("Outlook: Anhang konnte nicht angefügt werden: " + util::pathToUtf8(file.filename()), hr));
  }

  // Display first: Outlook adds the default signature, the text goes in front of it
  Variant modal = Variant::boolean(false);
  if (FAILED(hr = invoke(mail, DISPATCH_METHOD, L"Display", nullptr, {&modal})))
    return std::unexpected(failure("Outlook: E-Mail konnte nicht angezeigt werden", hr));
  Variant htmlBody;
  if (SUCCEEDED(invoke(mail, DISPATCH_PROPERTYGET, L"HTMLBody", &htmlBody)) && htmlBody.get()->vt == VT_BSTR) {
    Variant combined(insertAtBodyStart(htmlBody.text(), bodyAsHtml(draft.body)));
    hr = invoke(mail, DISPATCH_PROPERTYPUT, L"HTMLBody", nullptr, {&combined});
  } else {
    hr = put(mail, L"Body", draft.body);
  }
  if (FAILED(hr))
    return std::unexpected(failure("Outlook: Text konnte nicht gesetzt werden", hr));
  return {};
}

#else

util::Status createDraft(const exporting::MailDraft &) {
  return std::unexpected(util::Error{"Outlook ist nur unter Windows verfügbar"});
}

#endif

} // namespace st::app::outlook
