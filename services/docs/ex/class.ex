/**
 * \brief This is an object for "doing something"
 */
class DoSomething
{
		/// Do something cool!
	friend class DoSomethingCool;

		// Public members
    public:
		DoSomething();
		~DoSomething();

		int GetThings() const;

		// Private members
    private:
		ThingList theThings;	///< List of the things done
};
