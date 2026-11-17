#include <MadDisplay/MadDisplay.hpp>
#include <cassert>
#include <cstring>
#include <string>
#include <memory>
#include <limits>
#include <utility>
using namespace mad;

struct Tracked : Component {
  explicit Tracked(int& alive) : alive_(&alive) { ++*alive_; }
  Tracked(const Tracked& other) : Component(other),alive_(other.alive_) { ++*alive_; }
  ~Tracked() override { --*alive_; }
  Size preferredSize() const override { return {4,4}; }
  void draw(Canvas& canvas) const override { canvas.fillRect(bounds_,Color::On); }
  int* alive_;
};
struct Controller {
  Label label{"0"};
  Page page;
  int value = 0;
  void begin() {
    assert(page.addRoot(VerticalLayout().add(label,Button("+").onUse([this] { label.setText(++value); }))));
  }
};

static int freeFunctionUses = 0;
static void freeFunction() { ++freeFunctionUses; }

int main() {
  BareBuffer<128,64> screen;
  Display<BareBuffer<128,64>> display(screen);
  display.setSafeArea(Insets(0)); display.setNavigationAnimation(0,0);
  Label counter("0"); int value = 0;
  Page pg;
  assert(!pg.valid() && !display.show(pg));
  pg.draw(screen); pg.update(10); assert(!pg.dispatch(Action::Use));
  assert(pg.addRoot(VerticalLayout().setGap(4).add(
    counter,
    HorizontalLayout().setGap(2).add(
      Button("+").setWidth(Length::px(24)).onUse([&] { counter.setText(++value); }),
      Button("Reset").onClick([&] { value = 0; counter.setText(value); })
    )
  )));
  assert(display.show(pg)); display.update(0);
  assert(display.dispatch(Action::Use)); display.update(50000);
  assert(value == 1 && std::strcmp(counter.text(),"1") == 0);
  assert(display.dispatch(Action::Next) && display.dispatch(Action::Use));
  assert(value == 0);
  counter.setText(12345); display.update(100000);
  assert(counter.preferredSize().width == 29 && pg.root()->bounds().height == 64);
  assert(pg.addRoot(*pg.root())); // must retain ownership of the same root
  assert(display.show(pg)); display.update(150000);

  // Owned children survive builder destruction, copying and root replacement.
  int alive = 0;
  {
    Page owned;
    {
      auto tree = Panel().setGap(2).add(Tracked(alive),VerticalLayout().add(Tracked(alive)));
      assert(alive == 2 && tree.valid());
      assert(owned.addRoot(std::move(tree)));
    }
    assert(alive == 2);
    owned.draw(screen); assert(owned.root()->bounds().width == 128);
    Page copy = owned;
    assert(owned.addRoot(HorizontalLayout().add(Label("new"))));
    assert(alive == 2); copy.draw(screen);
  }
  assert(alive == 0);
  {
    Tracked named(alive);
    { Page borrowed; assert(borrowed.addRoot(VerticalLayout().add(named))); assert(alive == 1); borrowed.draw(screen); }
    assert(alive == 1);
  }
  assert(alive == 0);

  // Runtime replacement discards the old focused tree before freeing it.
  assert(pg.addRoot(GridLayout(2).setColumns(3).setGap(2).setColumnGap(4).add(Button("A"),Button("B"))));
  display.update(200000); assert(display.dispatch(Action::Use));
  assert(dynamic_cast<GridLayout*>(pg.root()) && dynamic_cast<GridLayout*>(pg.root())->columnGap() == 4);
  assert(pg.addRoot(FreeLayout().add(Label("A").setFrame(Frame::px(3,4,12,8)),Label("B"))));
  display.update(250000); assert(dynamic_cast<FreeLayout*>(pg.root()));

  Page positioned(FreeLayout().add(Label("position"),Frame::px(4,5,48,8)));
  positioned.draw(screen); assert(positioned.valid());
  bool positionedInk = false;
  for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) if (screen.isOn(x,y)) {
    positionedInk = true; assert(x >= 4 && x < 52 && y >= 5 && y < 13);
  }
  assert(positionedInk);

  // Single-child and repeated rvalue add() remain fluent; legacy bool add works.
  Page single(VerticalLayout().add(Label("A")).add(Button("B")));
  single.draw(screen); assert(single.valid());
  VerticalLayout namedLayout; Label named("X"); assert(namedLayout.add(named)); assert(!namedLayout.add(named));
  assert(namedLayout.add(Button("owned")));
  namedLayout.add(Label("C"),Button("D")).setGap(3); assert(namedLayout.gap() == 3);
  VerticalLayout parent,child; assert(parent.add(child)); assert(!child.add(parent));
  auto invalid = VerticalLayout().add(named,named); assert(!invalid.valid());
  Component* previous = pg.root(); assert(!pg.addRoot(std::move(invalid)) && pg.root() == previous);
  auto full = VerticalLayout().add();
  for (int i = 0; i < Layout::kMaxComponents; ++i) assert(full.add(Label("x")));
  assert(!full.add(Label("overflow")));
  assert(!std::move(full).add(Label("overflow")).valid());

  // A content control may own an entire temporary composition.
  Button content(HorizontalLayout().add(Label("one"),Label("two")));
  assert(content.valid() && content.preferredSize().width == 39);
  auto* originalContent = content.content(); assert(content.setContent(*originalContent));
  Button contentCopy(content); contentCopy.layout({0,0,128,64}); contentCopy.draw(screen);
  auto textButton = Button("hello").setText(123).setScale(2);
  Button copy(textButton); copy.setText("changed");
  assert(std::strcmp(static_cast<Label*>(textButton.content())->text(),"123") == 0);

  const Button constant("constant"); Button copiedConstant(std::move(constant));
  assert(copiedConstant.valid());
  const Page constantPage(VerticalLayout().add(Label("constant")));
  Page copiedPage(std::move(constantPage)); copiedPage.draw(screen); assert(copiedPage.valid());

  // Captures work on all controls; context callbacks and setters retain semantics.
  int uses = 0;
  Button legacy("legacy"); legacy.onUse([](void* context) { ++*static_cast<int*>(context); },&uses);
  assert(legacy.handle(Action::Use) && uses == 1);
  legacy.onUse([](void*) {}); assert(legacy.handle(Action::Use));
  auto check = Checkbox().setScale(2).setChecked(false).onUse([&] { ++uses; });
  auto toggle = Switch().setOn(false).setAnimationDuration(0).onUse([&] { ++uses; });
  auto radio = RadioButton().setSelected(false).onClick([&] { ++uses; });
  assert(check.handle(Action::Use) && check.checked());
  assert(toggle.handle(Action::Use) && toggle.on());
  assert(radio.handle(Action::Use) && radio.selected());
  assert(uses == 4); check.setChecked(false); toggle.setOn(false); assert(uses == 4);
  { Controller controller; controller.begin(); controller.page.draw(screen);
    assert(controller.page.dispatch(Action::Use) && controller.value == 1);
  }
  Button mutableCallback("mutable");
  int captured = 0;
  mutableCallback.onUse([captured,&uses]() mutable { uses = ++captured; });
  Button callbackCopy(mutableCallback);
  mutableCallback.handle(Action::Use); callbackCopy.handle(Action::Use); assert(uses == 2);
  {
    auto resource = std::make_shared<int>(9);
    std::weak_ptr<int> weak = resource;
    Button replacing("replace");
    replacing.onUse([resource,&replacing,&uses] {
      replacing.onUse([&uses] { ++uses; });
      assert(*resource == 9); // old capture remains alive until invocation returns
    });
    resource.reset(); replacing.handle(Action::Use); assert(weak.expired());
  }

  Button functionButton("function"); functionButton.onUse(freeFunction);
  assert(functionButton.handle(Action::Use) && freeFunctionUses == 1);
  struct MoveOnly {
    std::unique_ptr<int> value;
    int* result;
    MoveOnly(int* result) : value(new int(7)),result(result) {}
    MoveOnly(MoveOnly&&) = default;
    void operator()() { *result = *value; }
  };
  functionButton.onUse(MoveOnly(&uses)); functionButton.handle(Action::Use); assert(uses == 7);
  {
    Page replacement;
    assert(replacement.addRoot(Button("replace").onUse([&] {
      assert(replacement.addRoot(Label("done"))); ++uses;
    })));
    replacement.draw(screen);
    assert(replacement.dispatch(Action::Use) && uses == 8);
    replacement.draw(screen);
  }

  // Text owns C strings, temporary String-like values and numeric formatting.
  Label text("initial");
  { char local[] = "saved"; text.setText(local); local[0] = 'X'; }
  assert(std::strcmp(text.text(),"saved") == 0);
  text.setText(std::string("temporary")); assert(std::strcmp(text.text(),"temporary") == 0);
  Label stringLabel(std::string("owned")); Label labelCopy(stringLabel);
  stringLabel.setText("other"); assert(std::strcmp(labelCopy.text(),"owned") == 0);
  text.setText(std::numeric_limits<long long>::min()); assert(std::strcmp(text.text(),"-9223372036854775808") == 0);
  text.setText(std::numeric_limits<unsigned long long>::max()); assert(std::strcmp(text.text(),"18446744073709551615") == 0);
  text.setText(12.5,1); assert(std::strcmp(text.text(),"12.5") == 0);
  text.setText(text.text()+1); assert(std::strcmp(text.text(),"2.5") == 0);
  text.setText(static_cast<const char*>(nullptr)); assert(std::strcmp(text.text(),"") == 0);
  Button stringButton(std::string("String")); assert(stringButton.valid());
}
