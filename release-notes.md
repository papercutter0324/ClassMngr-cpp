# ClassMngr 0.12.0 Release Notes

ClassMngr 0.12.0 adds AI-assisted comment writing for speaking evaluations and
makes it easier to organize observations before creating reports. It also
streamlines common report and roster actions.

## AI-Assisted Speaking-Evaluation Comments

- Added tools to create AI comment prompts for individual E4–E6 students from
  private “Did Well” and “Needs Improvement” observations.
- Added a whole-class workflow that creates one prompt for selected students,
  parses the pasted AI response, and lets you review and selectively apply each
  comment.
- Replaces student names with placeholders in prompts and restores each
  student's name when comments are pasted back into ClassMngr.
- Added options to write comments directly to the student or in the third
  person.
- Added ChatGPT, Gemini, Claude, Microsoft Copilot, and a custom HTTPS website
  as preferred AI website choices.
- Added safeguards for missing observations, invalid responses, comment-length
  limits, and replacement of existing comments.

## Speaking-Evaluation Workflow

- Combined private observations and the report comment in a single student
  dialog.
- Split private observations into “Did Well” and “Needs Improvement” fields
  with automatic bullet-list formatting.
- Added comment character counters and consistent 450-character enforcement
  when typing or pasting text.
- Added AI prompt actions and editable comments to the report-creation dialog.
- Renamed report actions to “New Report” and “Print Reports” and added a
  separate “AI Comments for Class” action.

## Interface Improvements

- Added a Korean Keyboard shortcut to class rosters.
- Changed the signature preview in My Information to use a white background so
  transparent signatures are easier to see in both themes.
